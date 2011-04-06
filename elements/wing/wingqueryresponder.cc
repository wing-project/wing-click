/*
 * WINGQueryResponder.{cc,hh} 
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
#include "wingqueryresponder.hh"
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

WINGQueryResponder::WINGQueryResponder() :
	WINGBase<ReplyInfo>() {
}

WINGQueryResponder::~WINGQueryResponder() {
}

int WINGQueryResponder::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
				"ARP", cpkM, cpElementCast, "ARPTableMulti", &_arp_table, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	return 0;

}

void WINGQueryResponder::start_reply(PathMulti best, uint32_t seq) {

	int hops = best.size() - 1;
	NodeAddress src = best[hops].arr();
	NodeAddress dst = best[hops - 1].dep();

	if (_debug) {
		click_chatter("%{element} :: %s :: starting reply %s < %s seq %u next %u (%s)", 
				this,
				__func__, 
				best[0].dep().unparse().c_str(),
				best[hops].arr()._ip.unparse().c_str(),
				seq,
				hops - 1,
				route_to_string(best).c_str());
	}

	Packet * p = create_wing_packet(src, 
			dst, 
			WING_PT_REPLY, 
			NodeAddress(), 
			NodeAddress(), 
			NodeAddress(), 
			seq, 
			best,
			hops - 1);

	if (!p) {
		return;
	}

	output(0).push(p);

}

void WINGQueryResponder::process_query(Packet *p_in) {
	click_ether *eh = (click_ether *) p_in->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	if (pk->_type != WING_PT_QUERY) {
		click_chatter("%{element} :: %s :: bad packet_type %04x", 
				this,
				__func__, 
				_ip.unparse().c_str(), 
				pk->_type);
		p_in->kill();
		return;
	}
	IPAddress dst = pk->qdst();
	if (dst != _ip) {
		click_chatter("%{element} :: %s :: query not for me %s dst %s", 
				this,
				__func__, 
				_ip.unparse().c_str(), 
				dst.unparse().c_str());
		p_in->kill();
		return;
	}
	PathMulti best = _link_table->best_route(pk->qsrc(), false);
	if (!_link_table->valid_route(best)) {
		click_chatter("%{element} :: %s :: invalid route %s", 
				this,
				__func__, 
				route_to_string(best).c_str());
		p_in->kill();
		return;
	}
	ReplyInfo reply = ReplyInfo(pk->qsrc(), best);
	uint32_t seq = pk->seq();
	if (_debug) {
		click_chatter("%{element} :: %s :: generating reply %s seq %d", 
				this, 
				__func__,
				reply.unparse().c_str(), 
				seq);
	}
	/* process query */
	if (process_seen(reply, seq, false)) {
		/* start reply */
		start_reply(best, seq);
	}
	p_in->kill();
	return;
}

void WINGQueryResponder::process_reply(Packet *p_in) {
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
		if (!EtherAddress(eh->ether_dhost).is_group()) {
			/* 
			 * If the arp doesn't have a ethernet address, it
			 * will broadcast the packet. In this case,
			 * don't complain. But otherwise, something's up.
			 */
			click_chatter("%{element} :: %s :: data not for me %s hop %d/%d node %s (%s)",
					this, 
					__func__,
					_ip.unparse().c_str(), 
					pk->next(), 
					pk->num_links(),
					pk->get_link_dep(pk->next()).unparse().c_str(), 
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
	/* update the metrics from the packet */
	if (!update_link_table(p_in)) {
		p_in->kill();
		return;
	}
	if (pk->next() == 0) {
		/* I'm the ultimate consumer of this reply. */
		NodeAddress dst = pk->qdst();
		if (_debug) {
			click_chatter("%{element} :: %s :: got reply %s < %s seq %u (%s)", 
					this,
					__func__, 
					pk->get_link_dep(0).unparse().c_str(),
					pk->get_link_arr(pk->num_links() - 1).unparse().c_str(),
					pk->seq(),
					route_to_string(pk->get_path()).c_str());
		}
		p_in->kill();
		return;
	}
	/* Update pointer. */
	pk->set_next(pk->next() - 1);
	if (_debug) {
		click_chatter("%{element} :: %s :: forwarding reply %s < %s seq %u (%s)", 
				this,
				__func__, 
				pk->get_link_dep(pk->next()).unparse().c_str(),
				pk->get_link_arr(pk->next()).unparse().c_str(),
				pk->seq(),
				route_to_string(pk->get_path()).c_str());
	}
	/* set ethernet header */
	NodeAddress src = pk->get_link_arr(pk->next());
	NodeAddress dst = pk->get_link_dep(pk->next());
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

void WINGQueryResponder::push(int port, Packet *p_in) {
	if (port == 0) {
		process_query(p_in);
	} else if (port == 1) {
		process_reply(p_in);
	} else {
		p_in->kill();
	}	
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGQueryResponder)
