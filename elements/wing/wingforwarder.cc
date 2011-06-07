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
#include "wingforwarder.hh"
#include <click/args.hh>
CLICK_DECLS

WINGForwarder::WINGForwarder() :
	_inc_packets(0), _inc_bytes(0), _out_packets(0), _out_bytes(0) {
}

WINGForwarder::~WINGForwarder() {
}

int WINGForwarder::configure(Vector<String> &conf, ErrorHandler *errh) {

	return Args(conf, this, errh)
		.read_m("IP", _ip)
		.read_m("ARP", ElementCastArg("ARPTableMulti"), _arp_table)
		.complete();

}

void WINGForwarder::push(int, Packet *p_in) {
	WritablePacket *p = p_in->uniqueify();
	if (!p) {
		return;
	}
	click_ether *eh = (click_ether *) p_in->data();
	struct wing_data *pk = (struct wing_data *) (eh+1);
	if (pk->_type != WING_PT_DATA ) {
		click_chatter("%{element} :: %s :: bad packet_type %04x", 
				this,
				__func__, 
				pk->_type);
		p_in->kill();
		return;
	}
	if (pk->get_link_arr(pk->next())._ip != _ip) {
		if (!EtherAddress(eh->ether_dhost).is_group()) {
			/* 
			 * If the arp doesn't have a ethernet address, it
			 * will broadcast the packet. In this case,
			 * don't complain. But otherwise, something's up.
			 */
			click_chatter("%{element} :: %s :: data not for me %s hop %d/%d node %s route %s",
					this, 
					__func__,
					_ip.unparse().c_str(), 
					pk->next(), 
					pk->num_links(),
					pk->get_link_arr(pk->next()).unparse().c_str(), 
					route_to_string(pk->get_path()).c_str());
		}
		p->kill();
		return;
	}
	if (pk->next() >= pk->num_links()) {
		click_chatter("%{element} :: %s :: strange next=%d, nhops=%d", 
				this,
				__func__, 
				pk->next(), 
				pk->num_links());
		p_in->kill();
		return;
	}
	/* set the ip header pointer */
	const click_ip *ip = reinterpret_cast<const click_ip *>(pk->data());
	p->set_ip_header(ip, sizeof(click_ip));
	/* update incoming packets */
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
	// set ethernet header
	NodeAddress src = pk->get_link_dep(pk->next());
	NodeAddress dst = pk->get_link_arr(pk->next());
	EtherAddress eth_src = _arp_table->lookup(src);
	if (src && eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				src.unparse().c_str(),
				eth_src.unparse().c_str());
	}
	EtherAddress eth_dst = _arp_table->lookup(dst);
	if (dst && eth_dst.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for dst %s (%s)", 
				this,
				__func__,
				dst.unparse().c_str(),
				eth_dst.unparse().c_str());
	}
	memcpy(eh->ether_dhost, eth_dst.data(), 6);
	memcpy(eh->ether_shost, eth_src.data(), 6);
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
