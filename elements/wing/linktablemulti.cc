/*
 * linktablemulti.{cc,hh}
 * John Bicket, Stefano Testi, Roberto Riggio
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
#include "linktablemulti.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "pathmulti.hh"
#include <click/straccum.hh>
#include "nodeaddress.hh"
CLICK_DECLS

bool
cp_node_address(String s, NodeAddress *node)
{
	int l = s.find_left(':');
	IPAddress ip;
	int iface;
	if (!cp_ip_address(s.substring(0, l), &ip) || 
		!cp_integer(s.substring(l+1, s.length()), &iface)) {
		return false;
	}
	*node = NodeAddress(ip, iface);
	return true;
}

LinkTableMulti::LinkTableMulti() :
	_timer(this), _wcett(false), _beta(80) {
}

LinkTableMulti::~LinkTableMulti() {
}

int LinkTableMulti::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void LinkTableMulti::run_timer(Timer *) {
	clear_stale();
	dijkstra(true);
	dijkstra(false);
	_timer.schedule_after_msec(5000);
}

void *
LinkTableMulti::cast(const char *n) {
	if (strcmp(n, "LinkTableMulti") == 0)
		return (LinkTableMulti *) this;
	else
		return 0;
}

int LinkTableMulti::configure(Vector<String> &conf, ErrorHandler *errh) {
	int stale_period = 120;
	String devs;
	if (cp_va_kparse(conf, this, errh, 
			"IP", cpkM, cpIPAddress, &_ip,
			"DEV", cpkM, cpString, &devs,
			"WCETT", 0, cpBool, &_wcett, 
			"BETA", 0, cpUnsigned, &_beta, 
			"STALE", 0, cpUnsigned, &stale_period, 
			cpEnd) < 0)
		return -1;

	Vector<String> args;
	cp_spacevec(devs, args);
	if (args.size() < 1) {
		return errh->error("Error param DEV must have > 1 arg, given %u", args.size());
	}
	for (int x = 0; x < args.size(); x++) {
		Element *el = cp_element(args[x], router());
		if (!el || !el->cast("DevInfo")) {
			return errh->error("DEVS element %s is not a DevInfo", args[x].c_str());
		}
		_local_interfaces.insert(x + 1, (DevInfo *) el);
		HostInfo *nfo = _hosts.findp(_ip);
		if (!nfo) {
			_hosts.insert(_ip, HostInfo(_ip));
			nfo = _hosts.findp(_ip);
		}
		nfo->add_interface(x + 1);
	}
	_stale_timeout.assign(stale_period, 0);
	return 0;
}

void LinkTableMulti::take_state(Element *e, ErrorHandler *) {
	LinkTableMulti *q = (LinkTableMulti *) e->cast("LinkTableMulti");
	if (!q) {
		return;
	}
	_hosts = q->_hosts;
	_links = q->_links;
	dijkstra(true);
	dijkstra(false);
}

void LinkTableMulti::clear()
{
	_hosts.clear();
	_links.clear();
	_local_interfaces.clear();
}

bool LinkTableMulti::update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel)
{
	if (!from || !to || !metric) {
		return false;
	}
	if (_stale_timeout.sec() < (int) age) {
		return true;
	}
	/* make sure both the hosts exist */
	HostInfo *nfrom = _hosts.findp(from._ip);
	if (!nfrom) {
		_hosts.insert(from._ip, HostInfo(from._ip));
		nfrom = _hosts.findp(from._ip);
	}
	HostInfo *nto = _hosts.findp(to._ip);
	if (!nto) {
		_hosts.insert(to._ip, HostInfo(to._ip));
		nto = _hosts.findp(to._ip);
	}
	nfrom->add_interface(from._iface);
	nto->add_interface(to._iface);
	assert(nfrom);
	assert(nto);
	NodePair p = NodePair(from, to);
	LinkInfo *lnfo = _links.findp(p);
	if (!lnfo) {
		_links.insert(p, LinkInfo(from, to, seq, age, metric, channel));
	} else {
		lnfo->update(seq, age, metric, channel);
	}
	return true;
}

uint32_t LinkTableMulti::get_host_metric_to_me(IPAddress s) {
	if (!s) {
		return 0;
	}
	HostInfo *nfo = _hosts.findp(s);
	if (!nfo) {
		return 0;
	}
	return nfo->_metric_to_me;
}

uint32_t LinkTableMulti::get_host_metric_from_me(IPAddress s) {
	if (!s) {
		return 0;
	}
	HostInfo *nfo = _hosts.findp(s);
	if (!nfo) {
		return 0;
	}
	return nfo->_metric_from_me;
}

uint32_t LinkTableMulti::get_link_metric(NodeAddress from, NodeAddress to) {
	if (!from || !to) {
		return 0;
	}
	if (_blacklist.findp(from) || _blacklist.findp(to)) {
		return 0;
	}
	NodePair p = NodePair(from, to);
	LinkInfo *nfo = _links.findp(p);
	if (!nfo) {
		return 0;
	}
	return nfo->_metric;
}

uint32_t LinkTableMulti::get_link_channel(NodeAddress from, NodeAddress to) {
	if (!from || !to) {
		return 0;
	}
	if (_blacklist.findp(from) || _blacklist.findp(to)) {
		return 0;
	}
	NodePair p = NodePair(from, to);
	LinkInfo *nfo = _links.findp(p);
	if (!nfo) {
		return 0;
	}
	return nfo->_channel;
}

uint32_t LinkTableMulti::get_link_seq(NodeAddress from, NodeAddress to) {
	if (!from || !to) {
		return 0;
	}
	if (_blacklist.findp(from) || _blacklist.findp(to)) {
		return 0;
	}
	NodePair p = NodePair(from, to);
	LinkInfo *nfo = _links.findp(p);
	if (!nfo) {
		return 0;
	}
	return nfo->_seq;
}

uint32_t LinkTableMulti::get_link_age(NodeAddress from, NodeAddress to) {
	if (!from || !to) {
		return 0;
	}
	if (_blacklist.findp(from) || _blacklist.findp(to)) {
		return 0;
	}
	NodePair p = NodePair(from, to);
	LinkInfo *nfo = _links.findp(p);
	if (!nfo) {
		return 0;
	}
	return nfo->age();
}

uint32_t LinkTableMulti::get_route_metric(const PathMulti &route) {
	unsigned metric = 0;
	for (int i = 0; i < route.size() - 1; i++) {
		NodeAddress nfrom = NodeAddress(route[i]._ip, route[i]._dep);
		NodeAddress nto = NodeAddress(route[i+1]._ip, route[i+1]._arr);
		unsigned m = get_link_metric(nfrom, nto);
		if (m == 0) {
			return 0;
		}
		metric += m;
	}
	return metric;
}

String LinkTableMulti::route_to_string(PathMulti p) {
	StringAccum sa;
	int hops = p.size() - 1;
	int metric = 0;
	StringAccum sa2;
	for (int i = 0; i < p.size(); i++) {
		sa2 << p[i].unparse();
		if (i != p.size()-1) {
			NodeAddress nfrom = NodeAddress(p[i]._ip, p[i]._dep);
			NodeAddress nto = NodeAddress(p[i+1]._ip, p[i+1]._arr);
			int m = get_link_metric(nfrom, nto);
			sa2 << " (" << get_link_metric(nfrom, nto) << ", " << get_link_channel(nfrom, nto) << ") ";
			metric += m;
		}
	}
	sa << p[p.size()-1]._ip << " hops " << hops << " metric " << metric << " " << sa2;
	return sa.take_string();
}

bool LinkTableMulti::valid_route(const PathMulti &route) {
	if (route.size() < 1) {
		return false;
	}
	/* ensure the metrics are all valid */
	uint32_t metric = get_route_metric(route);
	if (metric == 0 || metric >= 999999) {
		return false;
	}
	/* ensure that a node appears no more than once */
	for (int x = 0; x < route.size(); x++) {
		for (int y = x + 1; y < route.size(); y++) {
			if (route[x] == route[y]) {
				return false;
			}
		}
	}
	return true;
}

PathMulti LinkTableMulti::best_route(IPAddress dst, bool from_me)
{
	PathMulti reverse_route;
	if (!dst) {
		return reverse_route;
	}
	HostInfo *nfo = _hosts.findp(dst);
	uint16_t prev_if = 0;
	if (from_me) {
		while (nfo && nfo->_metric_from_me != 0) {
			reverse_route.push_back(NodeAirport(nfo->_ip, nfo->_if_from_me, prev_if));
			prev_if = nfo->_prev_from_me._iface;
			nfo = _hosts.findp(nfo->_prev_from_me._ip);
		}
		if (nfo && nfo->_metric_from_me == 0) {
			reverse_route.push_back(NodeAirport(nfo->_ip, 0, prev_if));
		}
	} else {
		while (nfo && nfo->_metric_to_me != 0) {
			reverse_route.push_back(NodeAirport(nfo->_ip, prev_if, nfo->_if_to_me));
			prev_if = nfo->_prev_to_me._iface;
			nfo = _hosts.findp(nfo->_prev_to_me._ip);
		}
		if (nfo && nfo->_metric_to_me == 0) {
			reverse_route.push_back(NodeAirport(nfo->_ip, prev_if, 0));
		}
	}
	if (from_me) {
		Vector<NodeAirport> route;
		for (int i=reverse_route.size() - 1; i >= 0; i--) {
			route.push_back(reverse_route[i]);
		}
		return route;
	}
	return reverse_route;
}

String LinkTableMulti::print_links() {
	StringAccum sa;
	for (LTIter iter = _links.begin(); iter.live(); iter++) {
		LinkInfo n = iter.value();
		sa << n.unparse();
		for (LTIter inner = _links.begin(); inner.live(); inner++) {
			LinkInfo m = inner.value();
			if (n._from == m._from || 
				n._from == m._to || 
				n._to == m._from || 
				n._to == m._to) {
				if (n._channel != m._channel) {
					sa << " clashes with " << m.unparse();
				}
			}
		}
		sa << "\n";
	}
	return sa.take_string();
}

static int ipaddr_sorter(const void *va, const void *vb, void *) {
	IPAddress *a = (IPAddress *)va, *b = (IPAddress *)vb;
	if (a->addr() == b->addr()) {
		return 0;
	}
	return (ntohl(a->addr()) < ntohl(b->addr())) ? -1 : 1;
}

String LinkTableMulti::print_routes(bool from_me)
{
	StringAccum sa;
	Vector<IPAddress> ip_addrs;
	for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
		ip_addrs.push_back(iter.key());
	}
	click_qsort(ip_addrs.begin(), ip_addrs.size(), sizeof(IPAddress), ipaddr_sorter);
	for (int x = 0; x < ip_addrs.size(); x++) {
		IPAddress ip = ip_addrs[x];
		PathMulti r = best_route(ip, from_me);
		if (valid_route(r)) {
			sa << route_to_string(r) << "\n";
		}
	}
	return sa.take_string();
}

String LinkTableMulti::print_hosts() {
	StringAccum sa;
	Vector<IPAddress> ip_addrs;
	for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
		ip_addrs.push_back(iter.key());
	}
	click_qsort(ip_addrs.begin(), ip_addrs.size(), sizeof(IPAddress), ipaddr_sorter);
	for (int x = 0; x < ip_addrs.size(); x++) {
		HostInfo * hnfo = _hosts.findp(ip_addrs[x]);
		sa << ip_addrs[x] << " interfaces:";
		for (int y=0; y < hnfo->_interfaces.size(); y++) {
			sa << " " << hnfo->_interfaces[y];
		}
		sa << "\n";
	}
	return sa.take_string();
}

void LinkTableMulti::clear_stale() {
	LTable links;
	for (LTIter iter = _links.begin(); iter.live(); iter++) {
		LinkInfo nfo = iter.value();
		if ((unsigned)_stale_timeout.sec() >= nfo.age()) {
			links.insert(NodePair(nfo._from, nfo._to), nfo);
		}
	}
	_links.clear();
	for (LTIter iter = links.begin(); iter.live(); iter++) {
		LinkInfo nfo = iter.value();
		_links.insert(NodePair(nfo._from, nfo._to), nfo);
	}
}

Vector<NodeAddress> LinkTableMulti::get_neighbors(NodeAddress node)
{
	Vector<NodeAddress> neighbors;
	for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
		NodeAddress neighbor = NodeAddress(iter.key(), node._iface);
		LinkInfo *lnfo = _links.findp(NodePair(node, neighbor));
		if (lnfo) {
			neighbors.push_back(neighbor);
		}
	}
	return neighbors;
}

void LinkTableMulti::dijkstra(bool from_me)
{
	Timestamp start = Timestamp::now();
	typedef HashMap<IPAddress, bool> IPMap;
	IPMap ip_addrs;
	typedef HashMap<uint16_t, bool> IFMap;
	for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
		ip_addrs.insert(iter.value()._ip, true);
	}
	for (IPMap::const_iterator iter = ip_addrs.begin(); iter.live(); iter++) {
		/* clear them all initially */
		HostInfo *n = _hosts.findp(iter.key());
		n->clear(from_me);
	}
	HostInfo *root_info = _hosts.findp(_ip);
	assert(root_info);
	if (from_me) {
		root_info->_prev_from_me = NodeAddress(root_info->_ip,0);
		root_info->_metric_from_me = 0;
	} else {
		root_info->_prev_to_me = NodeAddress(root_info->_ip,0);
		root_info->_metric_to_me = 0;
	}
	IPAddress current_min_ip = root_info->_ip;
	while (current_min_ip) {
		HostInfo *current_min = _hosts.findp(current_min_ip);
		assert(current_min);
		if (from_me) {
			current_min->_marked_from_me = true;
		} else {
			current_min->_marked_to_me = true;
		}
		for (int i_ifcur = 0; i_ifcur < current_min->_interfaces.size(); i_ifcur++) {
			for (IPMap::const_iterator i = ip_addrs.begin(); i.live(); i++) {
				HostInfo *neighbor = _hosts.findp(i.key());
				for (int i_ifnei = 0; (i_ifnei < neighbor->_interfaces.size()) && (current_min_ip != i.key()); i_ifnei++) {
					assert(neighbor);
					bool marked = neighbor->_marked_to_me;
					if (from_me) {
						marked = neighbor->_marked_from_me;
					}
					if (marked) {
						continue;
					}
					NodeAddress ifnei = NodeAddress(neighbor->_ip,neighbor->_interfaces[i_ifnei]);
					NodeAddress ifcur = NodeAddress(current_min_ip,current_min->_interfaces[i_ifcur]);
					NodePair pair = NodePair(ifnei, ifcur);
					if (from_me) {
						pair = NodePair(ifcur, ifnei);
					}
					LinkInfo *lnfo = _links.findp(pair);
					if (!lnfo || !lnfo->_metric) {
						continue;
					}
					uint32_t link_channel = (lnfo->_channel > 0) ? lnfo->_channel : neighbor->_interfaces[i_ifnei];
					uint32_t neighbor_metric = neighbor->_metric_to_me;
					uint32_t current_metric = current_min->_metric_to_me;
					if (from_me) {
						neighbor_metric = neighbor->_metric_from_me;
						current_metric = current_min->_metric_from_me;
					}
					uint32_t adjusted_metric;
					if (_wcett) {
						uint32_t max_metric = 0;
						uint32_t total_ett = lnfo->_metric;
						bool ch_found = false;
						MetricTable * metric_table;
						if (from_me) {
							metric_table = &(current_min->_metric_table_from_me);
						} else {
							metric_table = &(current_min->_metric_table_to_me);
						}
						for (MetricIter it_metric = metric_table->begin(); it_metric.live(); it_metric++) {
							uint32_t actual_metric = 0;
							if (it_metric.key() == link_channel) {
								ch_found = true;
								actual_metric = it_metric.value() + lnfo->_metric;
							} else {
								actual_metric = it_metric.value();
							}
							if (actual_metric > max_metric) {
								max_metric = actual_metric;
							}
							total_ett += it_metric.value();
						}
						if ((!ch_found) && (lnfo->_metric > max_metric)) {
							max_metric = lnfo->_metric;
						}
						adjusted_metric = ((100 - _beta) * total_ett + _beta * max_metric) / 100;
					} else {
						adjusted_metric = current_metric + lnfo->_metric;
					}
			
					if (!neighbor_metric || adjusted_metric < neighbor_metric) {
						if (from_me) {
							neighbor->_metric_from_me = adjusted_metric;
							neighbor->_prev_from_me = NodeAddress(current_min_ip,current_min->_interfaces[i_ifcur]);
							neighbor->_if_from_me = neighbor->_interfaces[i_ifnei];
							if (_wcett) {
								neighbor->_metric_table_from_me.clear();
								for (MetricIter it_metric = current_min->_metric_table_from_me.begin(); 
									it_metric.live(); it_metric++) {
									neighbor->_metric_table_from_me.insert(it_metric.key(),it_metric.value());
								}
								uint32_t* ch_metric = neighbor->_metric_table_from_me.findp(link_channel);
								if (!ch_metric){
									neighbor->_metric_table_from_me.insert(link_channel,lnfo->_metric);
								} else {
									*ch_metric = *ch_metric + lnfo->_metric;
								}
							}
						} else {
							neighbor->_metric_to_me = adjusted_metric;
							neighbor->_prev_to_me = NodeAddress(current_min_ip,current_min->_interfaces[i_ifcur]);
							neighbor->_if_to_me = neighbor->_interfaces[i_ifnei];
							if (_wcett) {
								neighbor->_metric_table_to_me.clear();
								for (MetricIter it_metric = current_min->_metric_table_to_me.begin(); 
									it_metric.live(); it_metric++) {
									neighbor->_metric_table_to_me.insert(it_metric.key(),it_metric.value());
								}
								uint32_t* ch_metric = neighbor->_metric_table_to_me.findp(link_channel);
								if (!ch_metric){
									neighbor->_metric_table_to_me.insert(link_channel,lnfo->_metric);
								} else {
									*ch_metric = *ch_metric + lnfo->_metric;
								}
							}
						}
					}
				}
			}
		}
		current_min_ip = IPAddress();
		uint32_t  min_metric = ~0;
		for (IPMap::const_iterator i = ip_addrs.begin(); i.live(); i++) {
			HostInfo *nfo = _hosts.findp(i.key());
			uint32_t metric = nfo->_metric_to_me;
			bool marked = nfo->_marked_to_me;
			if (from_me) {
				metric = nfo->_metric_from_me;
				marked = nfo->_marked_from_me;
			}
			if (!marked && metric && metric < min_metric) {	
				current_min_ip = nfo->_ip;
				min_metric = metric;
			}
		}
	}
}

enum {
	H_IP,
	H_BLACKLIST,
	H_BLACKLIST_CLEAR,
	H_BLACKLIST_ADD,
	H_BLACKLIST_REMOVE,
	H_LINKS,
	H_ROUTES_FROM,
	H_ROUTES_TO,
	H_HOSTS,
	H_CLEAR,
	H_DIJKSTRA,
	H_DIJKSTRA_TIME,
	H_INSERT
};

String LinkTableMulti::read_handler(Element *e, void *thunk) {
	LinkTableMulti *td = (LinkTableMulti *) e;
	switch ((uintptr_t) thunk) {
	case H_IP:
		return td->_ip.unparse() + "\n";
	case H_BLACKLIST: {
		StringAccum sa;
		for (NodeIter iter = td->_blacklist.begin(); iter.live(); iter++) {
			sa << iter.value() << " ";
		}
		return sa.take_string() + "\n";
	}
	case H_LINKS:
		return td->print_links();
	case H_ROUTES_TO:
		return td->print_routes(false);
	case H_ROUTES_FROM:
		return td->print_routes(true);
	case H_HOSTS:
		return td->print_hosts();
	case H_DIJKSTRA_TIME: {
		StringAccum sa;
		sa << td->_dijkstra_time << "\n";
		return sa.take_string();
	}
	default:
		return String();
	}
	return 0;
}

int LinkTableMulti::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler * errh) {
	LinkTableMulti *f = (LinkTableMulti *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_INSERT: {
		Vector<String> args;
		NodeAddress from;
		NodeAddress to;
		uint32_t seq;
		uint32_t age;
		uint32_t metric;
		uint32_t channel;
		cp_spacevec(in_s, args);
		if (args.size() != 6) {
			return errh->error("must have six arguments, %u given", args.size());
		}
		if (!cp_node_address(args[0], &from)) {
			return errh->error("Couldn't read from node address");
		}
		if (!cp_node_address(args[1], &to)) {
			return errh->error("Couldn't read to node adddress");
		}
		if (!cp_unsigned(args[2], &metric)) {
			return errh->error("Couldn't read metric");
		}
		if (!cp_unsigned(args[3], &seq)) {
			return errh->error("Couldn't read seq");
		}
		if (!cp_unsigned(args[4], &age)) {
			return errh->error("Couldn't read age");
		}
		if (!cp_unsigned(args[5], &channel)) {
			return errh->error("Couldn't read channel");
		}
		f->update_link(from, to, seq, age, metric, channel);
		break;
	}
	case H_BLACKLIST_CLEAR: {
		f->_blacklist.clear();
		break;
	}
	case H_CLEAR: {
		f->clear();
		break;
	}
	case H_BLACKLIST_ADD: {
		if (!in_s) {
			return errh->error("no arguments provided");
		}
		Vector<String> args;
		IPAddress ip;
		uint32_t iface;
		cp_spacevec(in_s, args);
		if (args.size() != 2) {
			return errh->error("Must have two arguments (ip, iface): currently has %d: %s",
						args.size(), 
						args[0].c_str());
		}
		if (!cp_ip_address(args[0], &ip)) {
			return errh->error("Couldn't read IPAddress out of from");
		}
		if (!cp_integer(args[1], &iface)) {
			return errh->error("Couldn't read an  integer out of iface");
		}
		f->_blacklist.insert(NodeAddress(ip, iface), NodeAddress(ip, iface));
		break;
	}
	case H_BLACKLIST_REMOVE: {
		if (!in_s) {
			return errh->error("no arguments provided");
		}
		Vector<String> args;
		IPAddress ip;
		uint32_t iface;
		cp_spacevec(in_s, args);
		if (args.size() != 2) {
			return errh->error("Must have two arguments (ip, iface): currently has %d: %s",
						args.size(), 
						args[0].c_str());
		}
		if (!cp_ip_address(args[0], &ip)) {
			return errh->error("Couldn't read IPAddress out of from");
		}
		if (!cp_integer(args[1], &iface)) {
			return errh->error("Couldn't read an  integer out of iface");
		}
		f->_blacklist.erase(NodeAddress(ip, iface));
		break;
	}
	case H_DIJKSTRA:
		f->dijkstra(true);
		f->dijkstra(false);
		break;
	}
	return 0;
}

void LinkTableMulti::add_handlers() {
	add_read_handler("ip", read_handler, H_IP);
	add_read_handler("routes", read_handler, H_ROUTES_FROM);
	add_read_handler("routes_from", read_handler, H_ROUTES_FROM);
	add_read_handler("routes_to", read_handler, H_ROUTES_TO);
	add_read_handler("links", read_handler, H_LINKS);
	add_read_handler("hosts", read_handler, H_HOSTS);
	add_read_handler("blacklist", read_handler, H_BLACKLIST);
	add_read_handler("dijkstra_time", read_handler, H_DIJKSTRA_TIME);
	add_write_handler("clear", write_handler, H_CLEAR);
	add_write_handler("blacklist_clear", write_handler, H_BLACKLIST_CLEAR);
	add_write_handler("blacklist_add", write_handler, H_BLACKLIST_ADD);
	add_write_handler("blacklist_remove", write_handler, H_BLACKLIST_REMOVE);
	add_write_handler("dijkstra", write_handler, H_DIJKSTRA);
	add_write_handler("insert", write_handler, H_INSERT);
}

EXPORT_ELEMENT(LinkTableMulti)
CLICK_ENDDECLS
