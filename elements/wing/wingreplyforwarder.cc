/*
 * WINGReplyForwarder.{cc,hh} 
 * John Bicket, Roberto Riggio, Stefano Testi
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
#include "wingreplyforwarder.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
CLICK_DECLS

WINGReplyForwarder::WINGReplyForwarder() :
	_link_table(0), _arp_table(0) {
}

WINGReplyForwarder::~WINGReplyForwarder() {
}

int WINGReplyForwarder::configure(Vector<String> &conf, ErrorHandler *errh) {
	_debug = false;
	if (cp_va_kparse(conf, this, errh, "IP", cpkM, cpIPAddress, &_ip, "LT", cpkM, cpElement,
			&_link_table, "ARP", cpkM, cpElement, &_arp_table, "DEBUG", 0, cpBool, &_debug, cpEnd) < 0)
		return -1;

	if (_link_table->cast("LinkTableMulti") == 0)
		return errh->error("LT element is not a LinkTableMulti");
	if (_arp_table->cast("ARPTableMulti") == 0)
		return errh->error("ARP element is not a ARPTableMulti");

	return 0;
}

void WINGReplyForwarder::push(int, Packet *p_in) {
	WritablePacket *p = p_in->uniqueify();
	if (!p) {
		return;
	}
	click_ether *eh = (click_ether *) p_in->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	if (pk->_type != WING_PT_REPLY) {
		click_chatter("%{element} :: %s :: bad packet_type %04x", 
				this,
				__func__, 
				pk->_type);
		p_in->kill();
		return;
	}
	if (pk->get_link_dep(pk->next())._ip != _ip) {
		// It's not for me. these are supposed to be unicast, so how did this get to me?
		click_chatter("%{element} :: %s :: reply not for me %s hop %d/%d node %s eth %s (%s)", 
				this,
				__func__, 
				_ip.unparse().c_str(),
				pk->next(), 
				pk->num_links(), 
				pk->get_link_arr(pk->next()).unparse().c_str(),
				EtherAddress(eh->ether_dhost).unparse().c_str(),
				route_to_string(pk->get_path()).c_str());
		p_in->kill();
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
	/* update the metrics from the packet */
	for (int i = 0; i < pk->num_links(); i++) {
		NodeAddress a = pk->get_link_dep(i);
		NodeAddress b = pk->get_link_arr(i);
		uint32_t fwd_m = pk->get_link_fwd(i);
		uint32_t rev_m = pk->get_link_rev(i);
		uint32_t seq = pk->get_link_seq(i);
		uint32_t age = pk->get_link_age(i);
		uint32_t channel = pk->get_link_channel(i);
		if (!fwd_m || !rev_m || !seq || !channel) {
			click_chatter("%{element} :: %s :: invalid link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					rev_m,
					seq,
					channel,
					b.unparse().c_str());
			p_in->kill();
			return;
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					seq,
					age,
					fwd_m,
					channel,
					b.unparse().c_str());
		}
		if (fwd_m && !_link_table->update_link(a, b, seq, age, fwd_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update fwd_m %s > %d > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					b.unparse().c_str());
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					b.unparse().c_str(), 
					seq,
					age,
					rev_m,
					channel,
					a.unparse().c_str());
		}
		if (rev_m && !_link_table->update_link(b, a, seq, age, rev_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update rev_m %s < %d < %s",
					this, 
					__func__, 
					b.unparse().c_str(), 	
					rev_m,
					a.unparse().c_str());
		}
	}
	_link_table->dijkstra(true);
	/* I'm the ultimate consumer of this reply. */
	if (pk->next() == 0) {
		NodeAddress dst = pk->qdst();
		if (_debug) {
			click_chatter("%{element} :: %s :: got reply %s < %s seq %u (%s)", 
					this,
					__func__, 
					pk->get_link_dep(pk->next())._ip.unparse().c_str(),
					pk->get_link_arr(pk->num_links() - 1)._ip.unparse().c_str(),
					pk->seq(),
					route_to_string(pk->get_path()).c_str());
		}
		p_in->kill();
		return;
	}
	pk->set_next(pk->next() - 1);
	EtherAddress eth_dst = _arp_table->lookup(pk->get_link_dep(pk->next()));
	if (eth_dst.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for dst %s (%s)", 
				this,
				__func__, 
				pk->get_link_dep(pk->next()).unparse().c_str(),
				eth_dst.unparse().c_str());
		p_in->kill();
		return;
	}
	EtherAddress eth_src = _arp_table->lookup(pk->get_link_arr(pk->next()));
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__, 
				pk->get_link_arr(pk->next()).unparse().c_str(),
				eth_src.unparse().c_str());
		p_in->kill();
		return;
	}
	/* Forward the reply. */
	if (_debug) {
		click_chatter("%{element} :: %s :: forward reply %s < %s seq %u next %u (%s)", 
				this,
				__func__, 
				pk->get_link_dep(0)._ip.unparse().c_str(),
				pk->get_link_arr(pk->num_links() - 1)._ip.unparse().c_str(),
				pk->seq(),
				pk->next(),
				route_to_string(pk->get_path()).c_str());
	}
	memcpy(eh->ether_shost, eth_src.data(), 6);
	memcpy(eh->ether_dhost, eth_dst.data(), 6);
	output(0).push(p);
	return;
}

enum {
	H_DEBUG, H_IP
};

String WINGReplyForwarder::read_handler(Element *e, void *thunk) {
	WINGReplyForwarder *td = (WINGReplyForwarder *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_IP:
		return td->_ip.unparse() + "\n";
	default:
		return String();
	}
}

int WINGReplyForwarder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {
	WINGReplyForwarder *f = (WINGReplyForwarder *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void WINGReplyForwarder::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_read_handler("ip", read_handler, H_IP);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGReplyForwarder)
