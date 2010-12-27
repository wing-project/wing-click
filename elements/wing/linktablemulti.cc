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
#include <click/straccum.hh>
CLICK_DECLS

LinkTableMulti::LinkTableMulti()
{
}

LinkTableMulti::~LinkTableMulti()
{
}

void *
LinkTableMulti::cast(const char *n) {
	if (strcmp(n, "LinkTableMulti") == 0)
		return (LinkTableMulti *) this;
	else if (strcmp(n, "LinkTable") == 0)
		return (LinkTable *) this;
	else
		return 0;
}

int 
LinkTableMulti::configure(Vector<String> &conf, ErrorHandler *errh) {
	int stale_period = 120;
	String ifaces;
	if (cp_va_kparse(conf, this, errh, 
			"IP", cpkM, cpIPAddress, &_ip,
			"IFACES", cpkM, cpString, &ifaces,
			"WCETT", 0, cpBool, &_wcett, 
			"BETA", 0, cpUnsigned, &_beta, 
			"STALE", 0, cpUnsigned, &stale_period, 
			cpEnd) < 0)
		return -1;

	Vector<String> args;
	cp_spacevec(ifaces, args);
	if (args.size() < 1) {
		return errh->error("Error param IFACES must have > 0 arg, given %u", args.size());
	}
	HostInfoMulti *nfo = _hosts.findp(_ip);
	if (!nfo) {
		_hosts.insert(_ip, HostInfoMulti());
		nfo = _hosts.findp(_ip);
		nfo->_address = _ip;
	}
	for (int x = 0; x < args.size(); x++) {
		int iface;
		if (!cp_unsigned(args[x], &iface)) {
			errh->error("argument %s should be an unsigned", args[x].c_str());
		}
		nfo->add_interface(iface);
	}
	_stale_timeout.assign(stale_period, 0);
	return 0;
}

PathMulti 
LinkTableMulti::best_route(IPAddress dst, bool from_me)
{
	PathMulti reverse_route;
	if (!dst) {
		return reverse_route;
	}
	HostInfoMulti *nfo = _hosts.findp(dst);
	uint16_t prev_if = 0;
	if (from_me) {
		while (nfo && nfo->_metric_from_me != 0) {
			reverse_route.push_back(NodeAirport(nfo->_address, nfo->_if_from_me, prev_if));
			prev_if = nfo->_prev_from_me._iface;
			nfo = _hosts.findp(nfo->_prev_from_me._ip);
		}
		if (nfo && nfo->_metric_from_me == 0) {
			reverse_route.push_back(NodeAirport(nfo->_address, 0, prev_if));
		}
	} else {
		while (nfo && nfo->_metric_to_me != 0) {
			reverse_route.push_back(NodeAirport(nfo->_address, prev_if, nfo->_if_to_me));
			prev_if = nfo->_prev_to_me._iface;
			nfo = _hosts.findp(nfo->_prev_to_me._ip);
		}
		if (nfo && nfo->_metric_to_me == 0) {
			reverse_route.push_back(NodeAirport(nfo->_address, prev_if, 0));
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

String 
LinkTableMulti::route_to_string(PathMulti p) {
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

String 
LinkTableMulti::print_routes(bool from_me, bool)
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

uint32_t 
LinkTableMulti::get_route_metric(const PathMulti &route) {
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

void 
LinkTableMulti::dijkstra(bool from_me)
{
	Timestamp start = Timestamp::now();
	typedef HashMap<IPAddress, bool> IPMap;
	IPMap ip_addrs;
	typedef HashMap<uint16_t, bool> IFMap;
	for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
		ip_addrs.insert(iter.value()._address, true);
	}
	for (IPMap::const_iterator iter = ip_addrs.begin(); iter.live(); iter++) {
		/* clear them all initially */
		HostInfoMulti *n = _hosts.findp(iter.key());
		n->clear(from_me);
	}
	HostInfoMulti *root_info = _hosts.findp(_ip);
	assert(root_info);
	if (from_me) {
		root_info->_prev_from_me = NodeAddress(root_info->_address,0);
		root_info->_metric_from_me = 0;
	} else {
		root_info->_prev_to_me = NodeAddress(root_info->_address,0);
		root_info->_metric_to_me = 0;
	}
	IPAddress current_min_ip = root_info->_address;
	while (current_min_ip) {
		HostInfoMulti *current_min = _hosts.findp(current_min_ip);
		assert(current_min);
		if (from_me) {
			current_min->_marked_from_me = true;
		} else {
			current_min->_marked_to_me = true;
		}
		for (int i_ifcur = 0; i_ifcur < current_min->_interfaces.size(); i_ifcur++) {
			for (IPMap::const_iterator i = ip_addrs.begin(); i.live(); i++) {
				HostInfoMulti *neighbor = _hosts.findp(i.key());
				for (int i_ifnei = 0; (i_ifnei < neighbor->_interfaces.size()) && (current_min_ip != i.key()); i_ifnei++) {
					assert(neighbor);
					bool marked = neighbor->_marked_to_me;
					if (from_me) {
						marked = neighbor->_marked_from_me;
					}
					if (marked) {
						continue;
					}
					NodeAddress ifnei = NodeAddress(neighbor->_address,neighbor->_interfaces[i_ifnei]);
					NodeAddress ifcur = NodeAddress(current_min_ip,current_min->_interfaces[i_ifcur]);
					AddressPair<NodeAddress> pair = AddressPair<NodeAddress>(ifnei, ifcur);
					if (from_me) {
						pair = AddressPair<NodeAddress>(ifcur, ifnei);
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
						for (MTIter it_metric = metric_table->begin(); it_metric.live(); it_metric++) {
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
								for (MTIter it_metric = current_min->_metric_table_from_me.begin(); 
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
								for (MTIter it_metric = current_min->_metric_table_to_me.begin(); 
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
			HostInfoMulti *nfo = _hosts.findp(i.key());
			uint32_t metric = nfo->_metric_to_me;
			bool marked = nfo->_marked_to_me;
			if (from_me) {
				metric = nfo->_metric_from_me;
				marked = nfo->_marked_from_me;
			}
			if (!marked && metric && metric < min_metric) {	
				current_min_ip = nfo->_address;
				min_metric = metric;
			}
		}
	}
}

EXPORT_ELEMENT(LinkTableMulti)
CLICK_ENDDECLS
