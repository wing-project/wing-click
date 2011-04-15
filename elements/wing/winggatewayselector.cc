/*
 * winggatewayselector.{cc,hh}
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
#include "winggatewayselector.hh"
#include <click/confparse.hh>
CLICK_DECLS

WINGGatewaySelector::WINGGatewaySelector() :
	_hna_index(0), _period(5000), _expire(30000), _timer(this) {
	_seq = Timestamp::now().usec();
}

WINGGatewaySelector::~WINGGatewaySelector() {
}

int WINGGatewaySelector::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
				"ARP", cpkM, cpElementCast, "ARPTableMulti", &_arp_table, 
				"PERIOD", 0, cpUnsigned, &_period, 
				"EXPIRE", 0, cpUnsigned, &_expire, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	return 0;

}

int WINGGatewaySelector::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void WINGGatewaySelector::run_timer(Timer *) {

	// remove obsolete gateways
	GWTable new_table;
	Timestamp now = Timestamp::now();
	for (GWIter iter = _gateways.begin(); iter.live(); iter++) {
		GWInfo nfo = iter.value();
		Timestamp expire = nfo._last_update + Timestamp::make_msec(_expire);
		if (now < expire) {
			new_table.insert(iter.key(), nfo);
		}
	}
	_gateways.clear();
	for (GWIter iter = new_table.begin(); iter.live(); iter++) {
		GWInfo nfo = iter.value();
		_gateways.insert(iter.key(), nfo);
	}

	// send HNAs
	if (_hnas.size() > 0) {
		_hna_index = (_hna_index + 1) % _hnas.size();
		Vector<int> ifs = _link_table->get_local_interfaces();
		for (int i = 0; i < ifs.size(); i++) {
			start_ad(ifs[i]);
		}
		_seq++;
	}

	// schedule next transmission
	unsigned max_jitter = _period / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	Timestamp delay = Timestamp::make_msec(_period + j - max_jitter);
	_timer.schedule_at(Timestamp::now() + delay);

}

void WINGGatewaySelector::start_ad(int iface) {
	HNAIter itr =  _hnas.begin();
	for (unsigned i = 0; i < _hnas.size(); i++) {
		if (i == _hna_index) {
			break;
		}
		itr++;
	}
	HNAInfo hna = itr.key();
	if (_debug) {
		click_chatter("%{element} :: %s :: hna %s seq %d iface %u", 
				this, 
				__func__,
				hna.unparse().c_str(), 
				_seq,
				iface);
	}
	Packet * p = create_wing_packet(NodeAddress(_ip, iface), 
				NodeAddress(), 
				WING_PT_GATEWAY, 
				hna._dst, 
				hna._nm, 
				hna._gw, 
				_seq, 
				PathMulti(),
				0);
	if (!p) {
		return;
	}
	append_seen(hna, _seq);
	output(0).push(p);
}

void WINGGatewaySelector::forward_seen(int iface, Seen *s) {
	PathMulti best = _link_table->best_route(s->_seen._gw, false);
	if (_debug) {
		click_chatter("%{element} :: %s :: hna %s seq %d iface %u", 
				this, __func__,
				s->_seen.unparse().c_str(), 
				s->_seq,
				iface);
	}
	Packet * p = create_wing_packet(NodeAddress(_ip, iface), 
				NodeAddress(), 
				WING_PT_GATEWAY, 
				s->_seen._dst, 
				s->_seen._nm, 
				s->_seen._gw, 
				s->_seq, 
				best,
				0);
	if (!p) {
		return;
	}
	output(0).push(p);
}

IPAddress WINGGatewaySelector::best_gateway(IPAddress address) {
	IPAddress best_gw = IPAddress();
	int best_metric = 0;
	Timestamp now = Timestamp::now();
	for (GWIter iter = _gateways.begin(); iter.live(); iter++) {
		GWInfo nfo = iter.value();
		if (!nfo._hna.contains(address)) {
			continue;
		}
		Timestamp expire = nfo._last_update + Timestamp::make_msec(_expire);
		PathMulti p = _link_table->best_route(nfo._hna._gw, false);
		int metric = _link_table->get_route_metric(p);
		if (now < expire && metric && ((!best_metric) || best_metric > metric)) {
			GWTable new_table;
			best_gw = nfo._hna._gw;
			best_metric = metric;
		}
	}
	return best_gw;
}

void WINGGatewaySelector::push(int, Packet *p_in) {
	click_ether *eh = (click_ether *) p_in->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	if (pk->_type != WING_PT_GATEWAY) {
		click_chatter("%{element} :: %s :: bad packet type %d", 
				this,
				__func__, 
				pk->_type);
		p_in->kill();
		return;
	}
	HNAInfo hna = HNAInfo(pk->qdst(), pk->netmask(), pk->qsrc());
	uint32_t seq = pk->seq();
	if (_debug) {
		click_chatter("%{element} :: %s :: got hna %s seq %d", 
				this, 
				__func__,
				hna.unparse().c_str(), 
				seq);
	}

	/* update the metrics from the packet */
	if (!update_link_table(p_in)) {
		p_in->kill();
		return;
	}

	/* update gateways from the packet */
	GWInfo *nfo = _gateways.findp(hna);
	if (!nfo) {
		_gateways.insert(hna, GWInfo(hna));
		nfo = _gateways.findp(hna);
	}
	nfo->_last_update = Timestamp::now();
	nfo->_seen++;

	/* process hna */
	process_seen(hna, seq, true);
	p_in->kill();
	return;
}

String WINGGatewaySelector::print_gateway_stats() {
	StringAccum sa;
	Timestamp now = Timestamp::now();
	for (GWIter iter = _gateways.begin(); iter.live(); iter++) {
		GWInfo nfo = iter.value();
		PathMulti p = _link_table->best_route(nfo._hna._gw, false);
		sa << nfo.unparse() << " ";
		sa << "current_metric " << _link_table->get_route_metric(p) << "\n";
	}
	return sa.take_string();
}

enum {
	H_GATEWAY_STATS,
	H_IS_GATEWAY,
	H_HNA,
	H_HNA_ADD,
	H_HNA_DEL,
	H_HNA_CLEAR
};

int WINGGatewaySelector::hna_add(IPAddress addr, IPAddress mask, bool mapped) {
	_hnas.find_force(HNAInfo(addr, mask, _ip), mapped);
	return 0;
}

int WINGGatewaySelector::hna_del(IPAddress addr, IPAddress mask) {
	_hnas.erase(HNAInfo(addr, mask, _ip));
	return 0;
}

void WINGGatewaySelector::hna_clear() {
	_hnas.clear();
}

String WINGGatewaySelector::hnas() {
	StringAccum sa;
	for (HNAIter iter = _hnas.begin(); iter.live(); iter++) {
		HNAInfo hna = iter.key();
		sa << hna.unparse().c_str() << "\n";
	}
	return sa.take_string();
}

String WINGGatewaySelector::read_handler(Element *e, void *thunk) {
	WINGGatewaySelector *f = (WINGGatewaySelector *) e;
	switch ((uintptr_t) thunk) {
		case H_GATEWAY_STATS:
			return f->print_gateway_stats();
		case H_HNA: {
			return f->hnas();
		}
		case H_IS_GATEWAY: {
			return String(f->is_gateway()) + "\n";
		}
		default: {
			return WINGBase<HNAInfo>::read_handler(e, thunk);
		}
	}
}

int WINGGatewaySelector::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	WINGGatewaySelector *f = (WINGGatewaySelector *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
		case H_HNA_ADD: {
			Vector<String> args;
			cp_spacevec(s, args);
			if (args.size() != 1) {
				return errh->error("expected ADDR/MASK");
			}
			IPAddress addr;
			IPAddress mask;
			if (!cp_ip_prefix(args[0], &addr, &mask)) {
				return errh->error("error param %s: must be an ip prefix", args[0].c_str());
			}
			f->hna_add(addr, mask, true);
			break;
		}
		case H_HNA_DEL: {
			Vector<String> args;
			cp_spacevec(s, args);
			if (args.size() != 1) {
				return errh->error("one argument expected, %u given", args.size());
			}
			IPAddress addr;
			IPAddress mask;
			if (!cp_ip_prefix(args[0], &addr, &mask)) {
				return errh->error("error param %s: must be an ip prefix (ADDR/MASK)", args[0].c_str());
			}
			f->hna_del(addr, mask);
			break;
		}
		case H_HNA_CLEAR: {
			f->hna_clear();
			break;
		}
		default: {
			return WINGBase<HNAInfo>::write_handler(in_s, e, vparam, errh);
		}

	}
	return 0;
}

void WINGGatewaySelector::add_handlers() {
	WINGBase<HNAInfo>::add_handlers();
	add_read_handler("is_gateway", read_handler, (void *) H_IS_GATEWAY);
	add_read_handler("gateway_stats", read_handler, (void *) H_GATEWAY_STATS);
	add_read_handler("hna", read_handler, (void *) H_HNA);
	add_write_handler("hna_add", write_handler, (void *) H_HNA_ADD);
	add_write_handler("hna_del", write_handler, (void *) H_HNA_DEL);
	add_write_handler("hna_clear", write_handler, (void *) H_HNA_CLEAR);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(WINGBase)
EXPORT_ELEMENT(WINGGatewaySelector)
