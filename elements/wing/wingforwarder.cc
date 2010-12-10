/*
 * wingforwarder.{cc,hh} -- Source Route data path implementation
 * Robert Morris, John Bicket, Roberto Riggio, Stefano Testi
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2009 CREATE-NET
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "wingforwarder.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
CLICK_DECLS

WINGForwarder::WINGForwarder() :
	_inc_packets(0), _inc_bytes(0) {
}

WINGForwarder::~WINGForwarder() {
}

int WINGForwarder::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
				"ARP", cpkM, cpElementCast, "ARPTableMulti", &_arp_table, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	return 0;

}

void WINGForwarder::push(int, Packet *p_in)
{
	WritablePacket *p = p_in->uniqueify();
	if (!p) {
		return;
	}
	click_ether *eh = (click_ether *) p->data();
	struct wing_data *pk = (struct wing_data *) (eh+1);
	if (pk->_type != WING_PT_DATA ) {
		click_chatter("%{element} :: %s :: bad packet_type %04x",
			      this, 
			      __func__,
			      _ip.unparse().c_str(), 
			      pk->_type);
		p->kill();
		return;
	}
	if (pk->get_link_arr(pk->next())._ip != _ip) {
		if (!EtherAddress(eh->ether_dhost).is_group()) {
			/* 
			 * If the arp doesn't have a ethernet address, it
			 * will broadcast the packet. In this case,
			 * don't complain. But otherwise, something's up.
			 */
			click_chatter("%{element} :: %s :: data not for me %s hop %d/%d node %s eth %s",
					this, 
					__func__,
					_ip.unparse().c_str(), 
					pk->next(), 
					pk->num_links(),
					pk->get_link_arr(pk->next()).unparse().c_str(), 
					EtherAddress(eh->ether_dhost).unparse().c_str());
		}
		p->kill();
		return;
	}
	const click_ip *ip = reinterpret_cast<const click_ip *>(pk->data());
	p->set_ip_header(ip, sizeof(click_ip));
	if (pk->next() >= pk->num_links()) {
		click_chatter("%{element} :: %s :: strange next=%d, nhops=%d", 
				this,
				__func__, 
				pk->next(), 
				pk->num_links());
		p_in->kill();
		return;
	}
	// update incoming packets
	_inc_packets++;
	_inc_bytes += p->length();
	if (pk->next() == (pk->num_links()-1)){
		/* I am the ultimate consumer of this packet */
		SET_MISC_IP_ANNO(p, pk->get_link_dep(0));
		output(1).push(p);
		return;
	} 
	// update outgoing packets
	_out_packets++;
	_out_bytes += p->length();
	// update pointer
	pk->set_next(pk->next() + 1);
	set_ethernet_header(p, pk->get_link_dep(pk->next()), pk->get_link_arr(pk->next()));
	output(0).push(p);
}

String WINGForwarder::print_stats() {
	StringAccum sa;
	sa << "Incoming Packets: " << _inc_packets << " Incoming Bytes: " << _inc_bytes << "\n";
	sa << "Outgoing Packets: " << _inc_packets << " Outgoing Bytes: " << _inc_bytes << "\n";
	return sa.take_string();
}

enum {
	H_INC_PACKETS, 
	H_INC_BYTES,
	H_OUT_PACKETS, 
	H_OUT_BYTES,
	H_STATS
};

String WINGForwarder::read_handler(Element *e, void *thunk) {
	WINGForwarder *f = (WINGForwarder *) e;
	switch ((uintptr_t) thunk) {
	case H_INC_PACKETS:
		return String(f->inc_packets()) + "\n";
	case H_INC_BYTES:
		return String(f->inc_bytes()) + "\n";
	case H_OUT_PACKETS:
		return String(f->out_packets()) + "\n";
	case H_OUT_BYTES:
		return String(f->out_bytes()) + "\n";
	case H_STATS:
		return f->print_stats();
	}
	return String();
}

void WINGForwarder::add_handlers() {
	add_read_handler("stats", read_handler, H_STATS);
	add_read_handler("inc_packets", read_handler, H_INC_PACKETS);
	add_read_handler("inc_bytes", read_handler, H_INC_BYTES);
	add_read_handler("out_packets", read_handler, H_OUT_PACKETS);
	add_read_handler("out_bytes", read_handler, H_OUT_BYTES);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGForwarder)
