/*
 * sink.{cc,hh}
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
#include "sink.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/vector.hh>
#include "sapacket.hh"
#include "availablesensors.hh"
CLICK_DECLS

Sink::Sink()
{
}

Sink::~Sink()
{
}

int
Sink::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  ret = cp_va_kparse(conf, this, errh,
		     "SENSORS", 0, cpElement, &_as,
		     "DEBUG", 0, cpBool, &_debug,
		     cpEnd);

  if (!_as) 
    return errh->error("SENSORS not specified");
  if (_as->cast("AvailableSensors") == 0) 
    return errh->error("SENSORS element is not a AvailableSensors");
  return ret;
}

void
Sink::push(int, Packet *p_in)
{
  struct sapacket *pk = (struct sapacket *) p_in->data();
  uint32_t nb_samples = 0;
  uint32_t keystream = 0;
  if (pk->type() == SA_PT_PROBE) {
    nb_samples = 1;
    keystream = _as->get_keystream(pk->sensors());
  } else if ((pk->type() == SA_PT_PACK_P) || (pk->type() == SA_PT_PACK_N)) {
    Vector<uint8_t> *sensors = new Vector<uint8_t>();
    for (int i = 0; i < pk->sensors(); i++) {
      sensors->push_front(pk->get_sensor(i));
    }
    nb_samples = (pk->type() == SA_PT_PACK_P) ? pk->sensors() : _as->nb_sensors() - pk->sensors();
    keystream = (pk->type() == SA_PT_PACK_P) ? _as->get_keystream_p(sensors) : _as->get_keystream_n(sensors);
  } else {
     click_chatter("%{element} :: %s :: unknown packet type", this, __func__);
  }
  WritablePacket *wp = Packet::make(sizeof(sapacket));
  struct sapacket *p = (struct sapacket *) wp->data();
  memset(p, '\0', sizeof(sapacket));
  p->set_version(_version);
  p->set_type(SA_PT_SINK);
  p->set_sensors(nb_samples);
  p->set_msg(_as->msg());
  p->set_avg(sa_dec(pk->avg(), keystream));
  p->set_var(sa_dec(pk->var(), keystream));
  if (_debug) {
     Timestamp now = Timestamp::now();
    click_chatter("%{element} :: %s :: %s %u %u %u\n", this, __func__, now.unparse().c_str(), p->avg(), p->var(), p->sensors());
  }
  output(0).push(wp);
  p_in->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Sink)

