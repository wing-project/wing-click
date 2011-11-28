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
#include <click/args.hh>
#include "dyngw.hh"
CLICK_DECLS

WINGGatewaySelector::WINGGatewaySelector() :
	_period(10000), _expire(30000), _timer(this) {
	_seq = Timestamp::now().usec();
}

WINGGatewaySelector::~WINGGatewaySelector() {
}

int WINGGatewaySelector::configure(Vector<String> &conf, ErrorHandler *errh) {

	return Args(conf, this, errh)
		.read_m("IP", _ip)
		.read_m("LT", ElementCastArg("LinkTableMulti"), _link_table)
		.read_m("ARP", ElementCastArg("ARPTableMulti"), _arp_table)
		.read("PERIOD", _period)
		.read("EXPIRE", _expire)
		.read("DEBUG", _debug)
		.complete();

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
	Vector<int> ifs = _link_table->get_local_interfaces();
	for (int i = 0; i < ifs.size(); i++) {
		start_ad(ifs[i]);
	}
	_seq++;

	// schedule next transmission
	unsigned max_jitter = _period / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	_timer.schedule_after_msec(_period + j - max_jitter);

}

void WINGGatewaySelector::start_ad(int iface) {
	for (int x = 0; x < _hnas.size(); x++) {
		if (_debug) {
			click_chatter("%{element} :: %s :: hna %s seq %d iface %u", 
					this, 
					__func__,
					_hnas[x].unparse().c_str(), 
					_seq,
					iface);
		}
		Packet * p = create_wing_packet(NodeAddress(_ip, iface), 
					NodeAddress(), 
					WING_PT_GATEWAY, 
					_hnas[x]._dst, 
					_hnas[x]._nm, 
					_hnas[x]._gw, 
					_seq, 
					PathMulti(),
					0);
		if (!p) {
			return;
		}
		output(0).push(p);
	}
}

void WINGGatewaySelector::forward_seen(int iface, Seen *s) {
	PathMulti best = _link_table->best_route(s->_seen._gw, false);
	if (_debug) {
		click_chatter("%{element} :: %s :: hna %s seq %d iface %u", 
				this, 
				__func__,
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
	if (!_link_table->update_link_table(p_in)) {
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
	H_HNAS,
	H_HNA_ADD,
	H_HNA_DEL,
	H_HNAS_CLEAR
};

int WINGGatewaySelector::hna_add(IPAddress addr, IPAddress mask) {
	HNAInfo route = HNAInfo(addr, mask, _ip);
	for (Vector<HNAInfo>::iterator it = _hnas.begin(); it!=_hnas.end(); ++it) {
		if (*it == route) {
			return 1;
		}
	}
	_hnas.push_back(route);
	return 0;
}

int WINGGatewaySelector::hna_del(IPAddress addr, IPAddress mask) {
	HNAInfo route = HNAInfo(addr, mask, _ip);
	for (Vector<HNAInfo>::iterator it = _hnas.begin(); it != _hnas.end();) {
		(*it == route) ? it = _hnas.erase(it) : ++it;
	}
	return 0;
}

void WINGGatewaySelector::hnas_clear() {
	_hnas.clear();
}

String WINGGatewaySelector::hnas() {
	StringAccum sa;
	for (Vector<HNAInfo>::iterator it = _hnas.begin(); it != _hnas.end();) {
		sa << it->unparse().c_str() << "\n";
		it++;
	}
	return sa.take_string();
}

String WINGGatewaySelector::read_handler(Element *e, void *thunk) {
	WINGGatewaySelector *f = (WINGGatewaySelector *) e;
	switch ((uintptr_t) thunk) {
		case H_GATEWAY_STATS:
			return f->print_gateway_stats();
		case H_HNAS: {
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
			if (f->_dyn_gw && f->_dyn_gw->enabled()) {
				return errh->error("dynamic gateway selection active");
			}
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
			f->hna_add(addr, mask);
			break;
		}
		case H_HNA_DEL: {
			if (f->_dyn_gw && f->_dyn_gw->enabled()) {
				return errh->error("dynamic gateway selection active");
			}
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
		case H_HNAS_CLEAR: {
			if (f->_dyn_gw && f->_dyn_gw->enabled()) {
				return errh->error("dynamic gateway selection active");
			}
			f->hnas_clear();
			break;
		}
	}
	return 0;
}

void WINGGatewaySelector::add_handlers() {
	WINGBase<HNAInfo>::add_handlers();
	add_read_handler("is_gateway", read_handler, (void *) H_IS_GATEWAY);
	add_read_handler("gateway_stats", read_handler, (void *) H_GATEWAY_STATS);
	add_read_handler("hnas", read_handler, (void *) H_HNAS);
	add_write_handler("hna_add", write_handler, (void *) H_HNA_ADD);
	add_write_handler("hna_del", write_handler, (void *) H_HNA_DEL);
	add_write_handler("hnas_clear", write_handler, (void *) H_HNAS_CLEAR);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(WINGBase)
EXPORT_ELEMENT(WINGGatewaySelector)
