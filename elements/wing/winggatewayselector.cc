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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
#include "winggatewayselector.hh"
CLICK_DECLS

WINGGatewaySelector::WINGGatewaySelector() :
	_ip(), _link_table(0), _arp_table(0), _hna_index(0), _jitter(1000), _max_seen_size(100), _period(5000), _timer(this), _debug(false) {
	_seq = Timestamp::now().usec();
}

WINGGatewaySelector::~WINGGatewaySelector() {
}

int WINGGatewaySelector::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"LT", 0, cpElement, &_link_table,
				"ARP", 0, cpElement, &_arp_table, 
				"PERIOD", 0, cpUnsigned, &_period, 
				"JITTER", 0, cpUnsigned, &_jitter, 
				"EXPIRE", 0, cpUnsigned, &_expire, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	if (_link_table && _link_table->cast("LinkTableMulti") == 0)
		return errh->error("LT element is not a LinkTableMulti");
	if (_arp_table && _arp_table->cast("ARPTableMulti") == 0)
		return errh->error("ARP element is not an ARPTableMulti");

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
		if (_hna_index < _hnas.size()) {
			Vector<int> ifs = _link_table->get_local_interfaces();
			for (int i = 0; i < ifs.size(); i++) {
				start_ad(ifs[i]);
			}
			_seq++;
		}
	}

	// schedule next transmission
	unsigned max_jitter = _period / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	Timestamp delay = Timestamp::make_msec(_period + j - max_jitter);
	_timer.schedule_at(Timestamp::now() + delay);

}

void WINGGatewaySelector::start_ad(int iface) {
	HNAInfo nfo = _hnas[_hna_index];
	NodeAddress src = NodeAddress(_ip, iface);
	EtherAddress eth_src = _arp_table->lookup(src);
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				src.unparse().c_str(),
				eth_src.unparse().c_str());

	}
	if (_debug) {
		click_chatter("%{element} :: %s :: hna %s seq %d iface %u", 
				this, __func__,
				nfo.unparse().c_str(), 
				_seq,
				iface);
	}
	int len = wing_packet::len_wo_data(0);
	WritablePacket *p = Packet::make(len + sizeof(click_ether));
	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!", this, __func__);
		return;
	}
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	memset(pk, '\0', len);
	pk->_type = WING_PT_GATEWAY;
	pk->set_qdst(nfo._dst);
	pk->set_netmask(nfo._nm);
	pk->set_qsrc(nfo._gw);
	pk->set_seq(_seq);
	memcpy(eh->ether_shost, eth_src.data(), 6);
	memset(eh->ether_dhost, 0xff, 6);
	output(0).push(p);
}

void WINGGatewaySelector::forward_ad_hook() {
	Timestamp now = Timestamp::now();
	Vector<int> ifs = _link_table->get_local_interfaces();
	for (int x = 0; x < _seen.size(); x++) {
		if (_seen[x]._to_send < now && !_seen[x]._forwarded) {
			_link_table->dijkstra(false);
			for (int i = 0; i < ifs.size(); i++) {	
				forward_ad(ifs[i], &_seen[x]);
			}
			_seen[x]._forwarded = true;
		}
	}
}

void WINGGatewaySelector::forward_ad(int iface, Seen *s) {
	NodeAddress src = NodeAddress(_ip, iface);
	EtherAddress eth_src = _arp_table->lookup(src);
	if (eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				src.unparse().c_str(),
				eth_src.unparse().c_str());

	}
	PathMulti best = _link_table->best_route(s->_hna._gw, false);
	if (!_link_table->valid_route(best)) {
		click_chatter("%{element} :: %s :: invalid route from %s", 
				this,
				__func__, 
				s->_hna._gw.unparse().c_str());
		return;
	}
	if (_debug) {
		click_chatter("%{element} :: %s :: hna %s seq %d iface %u", 
				this, __func__,
				s->_hna.unparse().c_str(), 
				s->_seq,
				iface);
	}
	int hops = best.size() - 1;
	int len = wing_packet::len_wo_data(hops);
	WritablePacket *p = Packet::make(len + sizeof(click_ether));
	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!", this, __func__);
		return;
	}
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	memset(pk, '\0', len);
	pk->_type = WING_PT_GATEWAY;
	pk->set_qdst(s->_hna._dst);
	pk->set_netmask(s->_hna._nm);
	pk->set_qsrc(s->_hna._gw);
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
	for (int i = 0; i < pk->num_links(); i++) {
		NodeAddress a = pk->get_link_dep(i);
		NodeAddress b = pk->get_link_arr(i);
		uint32_t fwd_m = pk->get_link_fwd(i);
		uint32_t rev_m = pk->get_link_rev(i);
		uint32_t seq = pk->get_link_seq(i);
		uint32_t channel = pk->get_link_channel(i);
		if (!fwd_m || !rev_m || !seq || !channel) {
			p_in->kill();
			return;
		}
		if (fwd_m && !_link_table->update_link(a, b, seq, 0, fwd_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update fwd_m %s > %d > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					b.unparse().c_str());
		}
		if (rev_m && !_link_table->update_link(b, a, seq, 0, rev_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update rev_m %s < %d < %s",
					this, 
					__func__, 
					b.unparse().c_str(), 	
					rev_m,
					a.unparse().c_str());
		}
	}
	int si = 0;
	for (si = 0; si < _seen.size(); si++) {
		if (hna == _seen[si]._hna && seq == _seen[si]._seq) {
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
		_seen.push_back(Seen(hna, seq));
	}		
	_seen[si]._count++;
	_seen[si]._when = Timestamp::now();
	GWInfo *nfo = _gateways.findp(hna);
	if (!nfo) {
		_gateways.insert(hna, GWInfo(hna));
		nfo = _gateways.findp(hna);
	}
	nfo->_last_update = Timestamp::now();
	nfo->_seen++;
	/* schedule timer */
	int delay = click_random(1, _jitter);
	_seen[si]._to_send = _seen[si]._when + Timestamp::make_msec(delay);
	_seen[si]._forwarded = false;
	Timer *t = new Timer(static_forward_ad_hook, (void *) this);
	t->initialize(this);
	t->schedule_after_msec(delay);
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

String WINGGatewaySelector::read_handler(Element *e, void *thunk) {
	WINGGatewaySelector *f = (WINGGatewaySelector *) e;
	switch ((uintptr_t) thunk) {
	case H_GATEWAY_STATS:
		return f->print_gateway_stats();
	case H_HNA: {
		StringAccum sa;
		for (int x = 0; x < f->_hnas.size(); x++) {
			sa << f->_hnas[x].unparse().c_str() << "\n";
		}
		return sa.take_string();
	}
	case H_IS_GATEWAY: {
		return String(f->is_gateway()) + "\n";
	}
	default:
		return String();
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
		HNAInfo route = HNAInfo(addr, mask, f->_ip);
		for (Vector<HNAInfo>::iterator it = f->_hnas.begin(); it!=f->_hnas.end(); ++it) {
			if (*it == route) {
				return 0;
			}
		}
		f->_hnas.push_back(route);
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
		HNAInfo route = HNAInfo(addr, mask, f->_ip);
		for (Vector<HNAInfo>::iterator it = f->_hnas.begin(); it != f->_hnas.end();) {
			(*it == route) ? it = f->_hnas.erase(it) : ++it;
		}
		break;
	}
	case H_HNA_CLEAR: {
		f->_hnas.clear();
		break;
	}
	case H_IS_GATEWAY: {
		bool b;
		if (!cp_bool(s, &b)) {
			return errh->error("is_gateway parameter must be boolean");
		}
		HNAInfo route = HNAInfo(IPAddress(), IPAddress(), f->_ip);
		if (b) {
			for (Vector<HNAInfo>::iterator it = f->_hnas.begin(); it!=f->_hnas.end(); ++it) {
				if (*it == route) {
					return 0;
				}
			}
			f->_hnas.push_back(route);
		} else {
			for (Vector<HNAInfo>::iterator it = f->_hnas.begin(); it != f->_hnas.end();) {
				(*it == route) ? it = f->_hnas.erase(it) : ++it;
			}
		}
		break;
	}
	}
	return 0;
}

void WINGGatewaySelector::add_handlers() {
	add_read_handler("is_gateway", read_handler, (void *) H_IS_GATEWAY);
	add_read_handler("gateway_stats", read_handler, (void *) H_GATEWAY_STATS);
	add_read_handler("hna", read_handler, (void *) H_HNA);
	add_write_handler("is_gateway", write_handler, (void *) H_IS_GATEWAY);
	add_write_handler("hna_add", write_handler, (void *) H_HNA_ADD);
	add_write_handler("hna_del", write_handler, (void *) H_HNA_DEL);
	add_write_handler("hna_clear", write_handler, (void *) H_HNA_CLEAR);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGGatewaySelector)
