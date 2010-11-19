/*
 * aggregationpoint.{cc,hh}
 *
 * Roberto Riggio
 * Copyright (c) 2008, CREATE-NET
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions    
 * are met:
 * 
 *   - Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright 
 *     notice, this list of conditions and the following disclaimer in 
 *     the documentation and/or other materials provided with the 
 *     distribution.
 *   - Neither the name of the CREATE-NET nor the names of its 
 *     contributors may be used to endorse or promote products derived 
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <click/config.h>
#include "aggregationpoint.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/vector.hh>
#include "sapacket.hh"
#include "availablesensors.hh"
CLICK_DECLS

AggregationPoint::AggregationPoint() 
  :  _period(5000),
     _timer(this)
{
}

AggregationPoint::~AggregationPoint()
{
}

int 
AggregationPoint::configure(Vector<String>& conf, ErrorHandler* errh) 
{
	_debug = false;
	_pack = true;
	int ret = cp_va_kparse(conf, this, errh,
				"SENSORS", 0, cpElement, &_as,
				"PERIOD", 0, cpUnsigned, &_period,
				"DEBUG", 0, cpBool, &_debug,
				"PACK", 0, cpBool, &_pack,
				cpEnd);

	if (!_as) 
		return errh->error("SENSORS not specified");
	if (_as->cast("AvailableSensors") == 0) 
		return errh->error("SENSORS element is not a AvailableSensors");
	return ret;
}

int
AggregationPoint::initialize (ErrorHandler *errh)
{
  _timer.initialize(this);
  _timer.schedule_now();
  if (!(_signals = new NotifierSignal[ninputs()]))
    return errh->error("out of memory!");
  for (int i = 0; i < ninputs(); i++)
    _signals[i] = Notifier::upstream_empty_signal(this, i, 0);
  return 0;
}

void
AggregationPoint::send_packed()
{
    Vector<uint8_t> *sensors = new Vector<uint8_t>();
    uint32_t avg = 0;
    uint32_t var = 0;
    int n = ninputs();
    for (int i = 0; i < n; i++) {
      Packet *p = (_signals[i] ? input(i).pull() : 0);
      if (!p) {
        continue;
      }
      struct sapacket *pk = (struct sapacket *) p->data();
      if (_as->msg() != pk->msg()) {
        click_chatter("%{element} :: %s :: wrong packet overlay %u expected %u", this, __func__, pk->msg(), _as->msg());
        p->kill();
        continue;
      }
      if (pk->type() == SA_PT_PROBE) {
        sensors->push_front(pk->sensors());
        avg = sa_add_ciphers(avg, pk->avg());
        var = sa_add_ciphers(var, pk->var());
      } else if (pk->type() == SA_PT_PACK_P) {
        for (int i = 0; i < pk->sensors(); i++) {
          sensors->push_front(pk->get_sensor(i));
        }
        avg = sa_add_ciphers(avg, pk->avg());
        var = sa_add_ciphers(var, pk->var());
      } else {
        click_chatter("%{element} :: %s :: unknown packet type", this, __func__);
      }
      p->kill();
    }
    WritablePacket *q = Packet::make(sapacket::len(sensors->size()));
    struct sapacket *wp = (struct sapacket *) q->data();
    memset(wp, '\0', sapacket::len(1));
    wp->set_version(_version);
    wp->set_type(SA_PT_PACK_P);
    wp->set_sensors(sensors->size());
    wp->set_msg(_as->msg());
    wp->set_avg(avg);
    wp->set_var(var);
    for (int i = 0; i < sensors->size(); i++) {
      wp->set_sensor(i, sensors->at(i));
    }
    if (sensors->size() > 0) {
      output(0).push(q);
    }
}

void
AggregationPoint::send_unpacked()
{
  int n = ninputs();
  for (int i = 0; i < n; i++) {
    Packet *p = (_signals[i] ? input(i).pull() : 0);
    if (!p) {
      continue;
    }
    struct sapacket *pk = (struct sapacket *) p->data();
    if (_as->msg() != pk->msg()) {
      click_chatter("%{element} :: %s :: wrong packet overlay %u expected %u", this, __func__, pk->msg(), _as->msg());
      p->kill();
      continue;
    }
    if ((pk->type() != SA_PT_PROBE) && (pk->type() != SA_PT_PACK_P)) {
      click_chatter("%{element} :: %s :: unknown packet type", this, __func__);
      p->kill();
    } else {
      output(0).push(p);
    }
  }
}

void
AggregationPoint::run_timer(Timer *)
{
  if (_pack) 
    send_packed();
  else
    send_unpacked();
  // re-schedule timer
  unsigned max_jitter = _period / 100;
  unsigned j = click_random(0, 2 * max_jitter);
  Timestamp delay = Timestamp::make_msec(_period + j - max_jitter);
  _timer.reschedule_after(delay);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AggregationPoint)
