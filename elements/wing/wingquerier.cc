/*
 * WINGQuerier.{cc,hh}
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
#include "wingquerier.hh"
#include <click/args.hh>
CLICK_DECLS

bool
cp_node_airport(String s, NodeAirport *node)
{
	int l = s.find_left(':');
	int r = s.find_right(':');
	IPAddress ip;
	int arr, dep;
	if (!cp_integer(s.substring(0, l), &arr) ||
		!cp_ip_address(s.substring(l + 1, r - 2), &ip) ||
		!cp_integer(s.substring(r + 1, s.length()), &dep)) {
		return false;
	}
	*node = NodeAirport(ip, arr, dep);
	return true;
}

WINGQuerier::WINGQuerier() {
}

WINGQuerier::~WINGQuerier() {
}

int WINGQuerier::configure(Vector<String> &conf, ErrorHandler *errh) {

	_query_wait = Timestamp(5);
	_time_before_switch = Timestamp(5);

	return Args(conf, this, errh)
		.read_m("IP", _ip)
		.read_m("LT", ElementCastArg("LinkTableMulti"), _link_table)
		.read_m("ARP", ElementCastArg("ARPTableMulti"), _arp_table)
		.read("TIME_BEFORE_SWITCH", _time_before_switch)
		.read("QUERY_WAIT", _query_wait)
		.read("DEBUG", _debug)
		.complete();

}

Packet *
WINGQuerier::encap(Packet *p_in, PathMulti best)
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
		pk->set_link(i, best[i].dep(), best[i+1].arr());
	}

	NodeAddress src = best[0].dep();
	NodeAddress dst = best[1].arr();

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

	return p;

}

void
WINGQuerier::encap(Packet *p_in)
{

	int data_len = p_in->length();

	WritablePacket *p = p_in->push(sizeof(wing_bcast_data) + sizeof(click_ether));
	if (p == 0) {
		click_chatter("%{element} :: %s :: cannot encap packet!", this, __func__);
		return;
	}

	click_ether *eh = (click_ether *) p->data();
	struct wing_bcast_data *pk = (struct wing_bcast_data *) (eh + 1);
	memset(pk, '\0', sizeof(wing_bcast_data));

	pk->_type = WING_PT_BCAST_DATA;
	pk->set_data_len(data_len);

	Vector<int> ifs = _link_table->get_local_interfaces();

	EtherAddress eth_src = _arp_table->lookup(NodeAddress(_ip, ifs[0]));
	if (_ip && eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				_ip.unparse().c_str(),
				eth_src.unparse().c_str());
	}

	memset(eh->ether_dhost, 0xff, 6);
	memcpy(eh->ether_shost, eth_src.data(), 6);

	output(0).push(p);

	for (int i = 1; i < ifs.size(); i++) {
		if (Packet *q = p->clone()) {
			eh = (click_ether *) q->data();
			eth_src = _arp_table->lookup(NodeAddress(_ip, ifs[i]));
			if (_ip && eth_src.is_group()) {
				click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
						this,
						__func__,
						_ip.unparse().c_str(),
						eth_src.unparse().c_str());
			}
			memcpy(eh->ether_shost, eth_src.data(), 6);
			output(0).push(q);
		}
	}

}

void WINGQuerier::push(int, Packet *p_in) {
	IPAddress dst = p_in->dst_ip_anno();
	if (!dst) {	
		click_chatter("%{element} :: %s :: dst annotation not set %s", 
				this, 
				__func__,
				dst.unparse().c_str());
		p_in->kill();
		return;
	}
	/* look for static routes first */
	PathMulti *p = _routes.findp(dst);
	if (p) {
		p_in = encap(p_in, *p);
		if (p_in) {
			output(0).push(p_in);
		}
		return;
	}
	/* look known routes */
	DstInfo *nfo = _queries.findp(dst);
	if (!nfo) {
		_queries.insert(dst, DstInfo(dst));
		nfo = _queries.findp(dst);
		nfo->_best_metric = 0;
		nfo->_count = 0;
		nfo->_p = PathMulti();
		nfo->_last_query = Timestamp(0);
		nfo->_last_switch = Timestamp(0);
		nfo->_first_selected = Timestamp(0);
	}
	/* start a new query if the known one is too old */
	Timestamp now = Timestamp::now();
	Timestamp expire = nfo->_last_switch + _time_before_switch;
	if (!nfo->_best_metric || !nfo->_p.size() || expire < now) {
		PathMulti best = _link_table->best_route(dst, true);
		bool valid = _link_table->valid_route(best);
		nfo->_last_switch.assign_now();
		if (valid) {
			if (nfo->_p != best) {
				nfo->_first_selected.assign_now();
			}
			nfo->_p = best;
			nfo->_best_metric = _link_table->get_route_metric(best);
		} else {
			nfo->_p = PathMulti();
			nfo->_best_metric = 0;
		}
	}
	if (nfo->_best_metric) {
		p_in = encap(p_in, nfo->_p);
		if (p_in) {
			output(0).push(p_in);
		}
		return;
	}
	if (_debug) {
		click_chatter("%{element} :: %s :: no valid route to %s", 
				this,
				__func__, 
				dst.unparse().c_str());
	}
	if ((nfo->_last_query + _query_wait) < Timestamp::now()) {
		nfo->_last_query.assign_now();
		nfo->_count++;
		output(1).push(p_in);
		return;
	} 
	p_in->kill();
}

String WINGQuerier::print_queries() {
	StringAccum sa;
	Timestamp now = Timestamp::now();
	for (DstTable::const_iterator iter = _queries.begin(); iter.live(); iter++) {
		DstInfo dst = iter.value();
		PathMulti best = _link_table->best_route(dst._ip, true);
		int best_metric = _link_table->get_route_metric(best);
		sa << dst._ip;
		sa << " query_count " << dst._count;
		sa << " best_metric " << dst._best_metric;
		sa << " last_query_ago " << now - dst._last_query;
		sa << " first_selected_ago " << now - dst._first_selected;
		sa << " last_switch_ago " << now - dst._last_switch;
		sa << " current_path_metric " << _link_table->get_route_metric(dst._p);
		sa << " [ " << route_to_string(dst._p) << " ]";
		sa << " best_metric " << best_metric;
		sa << " best_route [ " << route_to_string(best) << " ]";
		sa << "\n";
	}
	return sa.take_string();
}

String WINGQuerier::print_routes() {
	StringAccum sa;
	for (RouteTable::iterator iter = _routes.begin(); iter.live(); iter++) {
		sa << iter.key().unparse() << " hops " << iter.value().size() - 1 << " " << route_to_string(iter.value()) << "\n";
	}
	return sa.take_string();
}

enum {
	H_RESET, H_QUERIES, H_ROUTES, H_CLEAR_ROUTES, H_CLEAR_QUERIES, H_ADD, H_DEL
};

String WINGQuerier::read_handler(Element *e, void *thunk) {
	WINGQuerier *c = (WINGQuerier *) e;
	switch ((intptr_t) (thunk)) {
	case H_QUERIES:
		return c->print_queries();
	case H_ROUTES:
		return c->print_routes();
	default:
		return "<error>\n";
	}
}

int WINGQuerier::write_handler(const String &in_s, Element *e, void *vparam,
		ErrorHandler *errh) {
	WINGQuerier *td = (WINGQuerier *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_ADD: {
		Vector<String> args;
		PathMulti p;
		cp_spacevec(s, args);
		if (args.size() < 2) {
			return errh->error("must have at least 2 arguments, %u given", args.size());
		}
		for (int x = 0; x < args.size(); x++) {
			NodeAirport node;
			if (!cp_node_airport(args[x], &node)) {
				return errh->error("unable to parse hop %s (arr:ipaddress:dep)", args[x].c_str());
			}
			p.push_back(node);
		}
		if (p[0]._ip != td->_ip) {
			return errh->error("first hop %s doesn't match my ip %s",
						p[0]._ip.unparse().c_str(),
						td->_ip.unparse().c_str());
		}
		td->_routes.insert(p[p.size() - 1]._ip, p);
		break;
	}
	case H_DEL: {
		IPAddress ip;
		if (!cp_ip_address(s, &ip)) {
			return errh->error("parameter %s must be IPAddress", s.c_str());
		}
		if (!td->_routes.remove(ip)) {
			return errh->error("unable to find destination %s", ip.unparse().c_str());
		}
		break;
	}
	case H_CLEAR_ROUTES: {
		td->_routes.clear();
		break;
	}
	case H_CLEAR_QUERIES: {
		td->_queries.clear();
		break;
	}
	}
	return 0;
}

void WINGQuerier::add_handlers() {
	add_read_handler("queries", read_handler, H_QUERIES);
	add_read_handler("static_routes", read_handler, H_ROUTES);
	add_write_handler("clear_static_routes", write_handler, H_CLEAR_ROUTES);
	add_write_handler("clear_queries", write_handler, H_CLEAR_QUERIES);
	add_write_handler("add_static_route", write_handler, H_ADD);
	add_write_handler("del_static_route", write_handler, H_DEL);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ARPTableMulti LinkTableMulti)
EXPORT_ELEMENT(WINGQuerier)
