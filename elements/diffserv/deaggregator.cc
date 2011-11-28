/*
 * deaggregator.{cc,hh}
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
#include "deaggregator.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
CLICK_DECLS

DeAggregator::DeAggregator()
{
}

DeAggregator::~DeAggregator()
{
}

int
DeAggregator::initialize(ErrorHandler*)
{
  _p = Packet::make(14);
  return 0;
}

int 
DeAggregator::configure(Vector<String>& conf, ErrorHandler* errh) 
{
  _et=0x0642;
  return cp_va_kparse(conf, this, errh,
			"ETHTYPE", 0, cpUnsignedShort, &_et,
			cpEnd);
}

void
DeAggregator::push(int, Packet *p)
{
  EtherAddress eth_dest;
  click_ether *eh = (click_ether *) p->data();
  if(eh->ether_type != htons(_et)) {
	click_chatter("%{element} :: %s :: bad ether_type %04x",
		      this, 
		      __func__,
		      ntohs(eh->ether_type));
	p->kill();
	return;
  }
  uint16_t np = 14;
  uint16_t npl = 0;
  while (np < p->length()) {
    memcpy(&npl, p->data()+np, 2);
    WritablePacket *wp = _p->clone()->uniqueify();
    wp = wp->put(ntohs(npl));
    EtherAddress dst = EtherAddress(p->ether_header()->ether_dhost);
    EtherAddress src = EtherAddress(p->ether_header()->ether_shost);
    memcpy(wp->data(), dst.data(), 6);
    memcpy(wp->data()+6, src.data(), 6);
    memcpy(wp->data()+12, p->data()+np+2,2);
    memcpy(wp->data()+14, p->data()+np+4,ntohs(npl));
    np += ntohs(npl)+4;
    output(0).push(wp);
  }
  p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DeAggregator)
