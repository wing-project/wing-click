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
	_link_table(0), _arp_table(0), _max_seen_size(100), _debug(false) {
}

WINGQueryResponder::~WINGQueryResponder() {
}

int WINGQueryResponder::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (cp_va_kparse(conf, this, errh, 
			"IP", cpkM, cpIPAddress, &_ip, 
			"LT", cpkM, cpElement, &_link_table,
			"ARP", cpkM, cpElement, &_arp_table, 
			"DEBUG", 0, cpBool, &_debug, 
			cpEnd) < 0)
		return -1;

	if (_link_table->cast("LinkTableMulti") == 0)
		return errh->error("LT element is not a LinkTableMulti");
	if (_arp_table->cast("ARPTableMulti") == 0)
		return errh->error("ARP element is not a ARPTableMulti");

	return 0;
}

void WINGQueryResponder::start_reply(PathMulti best, uint32_t seq) {
	int hops = best.size() - 1;
	EtherAddress eth_src = _arp_table->lookup(best[hops].arr());
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__, 
				best[hops].arr().unparse().c_str(),
				eth_src.unparse().c_str());
		return;
	}
	EtherAddress eth_dst = _arp_table->lookup(best[hops - 1].dep());
	if (eth_dst.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for dst %s (%s)", 
				this,
				__func__, 
				best[hops - 1].dep().unparse().c_str(),
				eth_dst.unparse().c_str());
		return;
	}
	int len = wing_packet::len_wo_data(hops);
	WritablePacket *p = Packet::make(len + sizeof(click_ether));
	if (p == 0) {
		return;
	}
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	memset(pk, '\0', len);
	pk->_type = WING_PT_REPLY;
	pk->set_seq(seq);
	pk->set_num_links(hops);
	pk->set_next(hops - 1);
	for (int i = 0; i < hops; i++) {
		pk->set_link(i, best[i].dep(), best[i + 1].arr(),
				_link_table->get_link_metric(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_metric(best[i + 1].arr(), best[i].dep()),
				_link_table->get_link_seq(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_age(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_channel(best[i].dep(), best[i + 1].arr()));
	}
	if (_debug) {
		click_chatter("%{element} :: %s :: starting reply %s < %s seq %u next %u (%s)", 
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
}

void WINGQueryResponder::push(int, Packet *p_in) {
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
	IPAddress src = pk->qsrc();
	uint32_t seq = pk->seq();
	_link_table->dijkstra(false);
	PathMulti best = _link_table->best_route(src, false);
	int si = 0;
	for (si = 0; si < _seen.size(); si++) {
		if (src == _seen[si]._src && seq == _seen[si]._seq) {
			break;
		}
	}
	if (si == _seen.size()) {
		if (_seen.size() >= _max_seen_size) {
			_seen.pop_front();
		}
		_seen.push_back(Seen(src, seq));
		si = _seen.size() - 1;
	}
	if (best == _seen[si]._last_response) {
		/*
		 * only send replies if the "best" path is different
		 * from the last reply
		 */
		if (_debug) {
			click_chatter("%{element} :: %s :: best path not different from last reply", this, __func__);
		}
		return;
	}
	_seen[si]._src = src;
	_seen[si]._seq = seq;
	_seen[si]._last_response = best;
	if (!_link_table->valid_route(best)) {
		click_chatter("%{element} :: %s :: invalid route for src %s: %s", 
				this,
				__func__, 
				src.unparse().c_str(), 
				route_to_string(best).c_str());
		return;
	}
	/* start reply */
	start_reply(best, seq);
	p_in->kill();
	return;
}

enum {
	H_DEBUG, H_IP
};

String WINGQueryResponder::read_handler(Element *e, void *thunk) {
	WINGQueryResponder *td = (WINGQueryResponder *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_IP:
		return td->_ip.unparse() + "\n";
	default:
		return String();
	}
}

int WINGQueryResponder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {
	WINGQueryResponder *f = (WINGQueryResponder *) e;
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

void WINGQueryResponder::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_read_handler("ip", read_handler, H_IP);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGQueryResponder)
