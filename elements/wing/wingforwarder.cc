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
	_inc_packets(0), _inc_bytes(0), _debug(false), _link_table(0), _arp_table(0) {
}

WINGForwarder::~WINGForwarder() {
}

int WINGForwarder::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (cp_va_kparse(conf, this, errh, "IP", cpkM, cpIPAddress, &_ip, "ARP", cpkM, cpElement,
			&_arp_table, "LT", cpkM, cpElement, &_link_table, "DEBUG", 0, cpBool, &_debug, cpEnd) < 0)
		return -1;
	if (_arp_table->cast("ARPTableMulti") == 0)
		return errh->error("ARPTable element is not a ARPTableMulti");
	if (_link_table->cast("LinkTableMulti") == 0)
		return errh->error("LT element is not a LinkTableMulti");
	return 0;
}

Packet *
WINGForwarder::encap(Packet *p_in, PathMulti best)
{
	if (best[0].dep()._ip != _ip) {
		click_chatter("%{element} :: %s :: first hop %s doesn't match my ip %s", 
				this,
				__func__,
				best[0].dep().unparse().c_str(), 
				_ip.unparse().c_str());
		p_in->kill();
		return 0;
	}
	EtherAddress eth_dst = _arp_table->lookup(best[1].arr());
	if (eth_dst.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for dst %s (%s)", 
				this,
				__func__,
				best[1].arr().unparse().c_str(),
				eth_dst.unparse().c_str());

	}
	EtherAddress eth_src = _arp_table->lookup(best[0].dep());
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				best[0].dep().unparse().c_str(),
				eth_src.unparse().c_str());
	}
	int hops = best.size() - 1;
	int len = wing_data::len_wo_data(hops);
	int data_len = p_in->length();
	WritablePacket *p = p_in->push(len + sizeof(click_ether));
	if (p == 0) {
		click_chatter("%{element} :: %s :: cannot encap packet!", this, __func__);
		return 0;
	}
	click_ether *eh = (click_ether *) p->data();
	struct wing_data *pk = (struct wing_data *) (eh + 1);
	memset(pk, '\0', len);
	pk->_type = WING_PT_DATA;
	pk->set_data_len(data_len);
	pk->set_num_links(hops);
	for (int i = 0; i < hops; i++) {
		pk->set_link(i, best[i].dep(), best[i+1].arr(), _link_table->get_link_channel(best[i].dep(), best[i+1].arr()));
	}
	memcpy(eh->ether_dhost, eth_dst.data(), 6);
	memcpy(eh->ether_shost, eth_src.data(), 6);
	return p;
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
	pk->set_next(pk->next() + 1);
	EtherAddress eth_dst = _arp_table->lookup(NodeAddress(pk->get_link_arr(pk->next())));
	if (eth_dst.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for dst %s (%s)", 
				this, 
				__func__,
				pk->get_link_arr(pk->next()).unparse().c_str(),
				eth_dst.unparse().c_str());
	}
	EtherAddress eth_src = _arp_table->lookup(NodeAddress(pk->get_link_dep(pk->next())));
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this, 
				__func__,
				pk->get_link_dep(pk->next()).unparse().c_str(),
				eth_src.unparse().c_str());
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
