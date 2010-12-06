/*
 * WINGMetricFlood.{cc,hh} 
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
#include "wingmetricflood.hh"
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

WINGMetricFlood::WINGMetricFlood() :
	_link_table(0), _arp_table(0), _jitter(1000), _max_seen_size(100), _debug(false) {
	_seq = Timestamp::now().usec();
}

WINGMetricFlood::~WINGMetricFlood() {
}

int WINGMetricFlood::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"LT", cpkM, cpElement, &_link_table, 
				"ARP", cpkM, cpElement, &_arp_table, 
				"JITTER", 0, cpUnsigned, &_jitter, 
				"DEBUG", 0, cpBool, &_debug, 
			cpEnd) < 0)
		return -1;

	if (_link_table->cast("LinkTableMulti") == 0)
		return errh->error("LinkTable element is not a LinkTableMulti");
	if (_arp_table->cast("ARPTableMulti") == 0)
		return errh->error("ARPTableMulti element is not a ARPTableMulti");

	return 0;

}

void WINGMetricFlood::forward_query_hook() {
	Timestamp now = Timestamp::now();
	Vector<int> ifs = _link_table->get_local_interfaces();
	for (int x = 0; x < _seen.size(); x++) {
		_link_table->dijkstra(false);
		if (_seen[x]._to_send < now && !_seen[x]._forwarded) {
			for (int i = 0; i < ifs.size(); i++) {
				forward_query(NodeAddress(_ip, ifs[i]), &_seen[x]);
			}
			_seen[x]._forwarded = true;
		}
	}
}

void WINGMetricFlood::forward_query(NodeAddress src, Seen *s) {
	EtherAddress eth_src = _arp_table->lookup(src);
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for %s",
			      this,
			      __func__,
			      src.unparse().c_str());
	}
	PathMulti best = _link_table->best_route(s->_src, false);
	if (!_link_table->valid_route(best)) {
		if (_debug) {
			click_chatter("%{element} :: %s :: invalid route from %s",
					this, 
					__func__, 
					s->_src.unparse().c_str());
		}
		return;
	}
	int hops = best.size() - 1;
	int len = wing_packet::len_wo_data(hops);
	WritablePacket *p = Packet::make(len + sizeof(click_ether));
	if (p == 0) {
		click_chatter("%{element} :: %s :: cannot make query packet!", this, __func__);
		return;
	}
	if (_debug) {
		click_chatter("%{element} :: %s :: forward query dst %s src %s seq %d iface %s", 
				this,
				__func__, 
				s->_dst.unparse().c_str(), 
				s->_dst.unparse().c_str(), 
				s->_seq,
				src.unparse().c_str());
	}
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	memset(pk, '\0', len);
	pk->_type = WING_PT_QUERY;
	pk->set_qdst(s->_dst);
	pk->set_qsrc(s->_src);
	pk->set_seq(s->_seq);
	pk->set_num_links(hops);
	for (int i = 0; i < hops; i++) {
		pk->set_link(i, best[i].dep(), best[i + 1].arr(),
				_link_table->get_link_metric(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_metric(best[i + 1].arr(), best[i].dep()),
				_link_table->get_link_seq(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_age(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_channel(best[i].dep(), best[i + 1].arr()));
	}
	memcpy(eh->ether_shost, eth_src.data(), 6);
	memset(eh->ether_dhost, 0xff, 6);
	output(0).push(p);
}

void WINGMetricFlood::process_flood(Packet *p_in) {
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
			click_chatter("%{element} :: %s :: invalid link %s > (%u, %u, %u, %u) > %s",
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
			click_chatter("%{element} :: %s :: invalid link %s > (%u, %u, %u, %u) > %s",
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
	IPAddress src = pk->qsrc();
	IPAddress dst = pk->qdst();
	uint32_t seq = pk->seq();
	int si = 0;
	for (si = 0; si < _seen.size(); si++) {
		if (src == _seen[si]._src && seq == _seen[si]._seq) {
			/* query already processed */
			_seen[si]._count++;
			p_in->kill();
			return;
		}
	}
	if (si == _seen.size()) {
		if (_seen.size() >= _max_seen_size) {
			_seen.pop_front();
			si--;
		}
		_seen.push_back(Seen(src, dst, seq));
	}
	_seen[si]._count++;
	_seen[si]._when = Timestamp::now();
	if (dst == _ip) {
		/* don't forward queries for me */
		/* just spit them out the output */
		output(1).push(p_in);
		return;
	}
	/* schedule timer */
	int delay = click_random(1, _jitter);
	_seen[si]._to_send = _seen[si]._when + Timestamp::make_msec(delay);
	_seen[si]._forwarded = false;
	Timer *t = new Timer(static_forward_query_hook, (void *) this);
	t->initialize(this);
	t->schedule_after_msec(delay);
	p_in->kill();
	return;
}

void WINGMetricFlood::start_query(IPAddress dst, NodeAddress src) {
	EtherAddress eth_src = _arp_table->lookup(src);
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for %s",
			      this,
			      __func__,
			      src.unparse().c_str());
	}
	unsigned len = wing_packet::len_wo_data(0);
	WritablePacket *p = Packet::make(len + sizeof(click_ether));
	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!", this, __func__);
		return;
	}
	if (_debug) {
		click_chatter("%{element} :: %s :: start query dst %s src %s seq %d iface %s", 
				this,
				__func__, 
				dst.unparse().c_str(), 
				src.unparse().c_str(), 
				_seq,
				src.unparse().c_str());
	}
	if (_seen.size() >= _max_seen_size) {
		_seen.pop_front();
	}
	_seen.push_back(Seen(src, dst, _seq));
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	memset(pk, '\0', len);
	pk->_type = WING_PT_QUERY;
	pk->set_qdst(dst);
	pk->set_qsrc(_ip);
	pk->set_seq(_seq);
	memcpy(eh->ether_shost, eth_src.data(), 6);
	memset(eh->ether_dhost, 0xff, 6);
	output(0).push(p);
}

void WINGMetricFlood::start_flood(Packet *p_in) {
	IPAddress dst = p_in->dst_ip_anno();
	p_in->kill();
	Vector<int> ifs = _link_table->get_local_interfaces();
	for (int i = 0; i < ifs.size(); i++) {
		start_query(dst, NodeAddress(_ip, ifs[i]));
	}
	_seq++;
}

void WINGMetricFlood::push(int port, Packet *p_in) {
	if (port == 0) {
		start_flood(p_in);
	} else if (port == 1) {
		process_flood(p_in);
	} else {
		p_in->kill();
	}	
}

enum {
	H_DEBUG, H_CLEAR, H_FLOODS
};

String WINGMetricFlood::read_handler(Element *e, void *thunk) {
	WINGMetricFlood *td = (WINGMetricFlood *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_FLOODS: {
		StringAccum sa;
		int x;
		for (x = 0; x < td->_seen.size(); x++) {
			sa << "src " << td->_seen[x]._src;
			sa << " dst " << td->_seen[x]._dst;
			sa << " seq " << td->_seen[x]._seq;
			sa << " count " << td->_seen[x]._count;
			sa << " forwarded " << td->_seen[x]._forwarded;
			sa << "\n";
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

int WINGMetricFlood::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	WINGMetricFlood *f = (WINGMetricFlood *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	case H_CLEAR:
		f->_seen.clear();
		break;
	}
	return 0;
}

void WINGMetricFlood::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("floods", read_handler, (void *) H_FLOODS);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
	add_write_handler("clear", write_handler, (void *) H_CLEAR);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGMetricFlood)
