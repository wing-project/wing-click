/*
 * LinkTable.{cc,hh} -- Routing Table in terms of links
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "linktable.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

LinkTable::LinkTable()
{
}

LinkTable::~LinkTable()
{
}

uint32_t
LinkTable::get_route_metric(const Path &route)
{
  unsigned metric = 0;
  for (int i = 0; i < route.size() - 1; i++) {
    unsigned m = get_link_metric(route[i], route[i+1]);
    if (m == 0) {
      return 0;
    }
    metric += m;
  }
  return metric;
}

Path
LinkTable::best_route(IPAddress dst, bool from_me)
{
  Path reverse_route;
  if (!dst) {
    return reverse_route;
  }
  HostInfo *nfo = _hosts.findp(dst);

  if (from_me) {
    while (nfo && nfo->_metric_from_me != 0) {
      reverse_route.push_back(nfo->_address);
      nfo = _hosts.findp(nfo->_prev_from_me);
    }
    if (nfo && nfo->_metric_from_me == 0) {
    reverse_route.push_back(nfo->_address);
    }
  } else {
    while (nfo && nfo->_metric_to_me != 0) {
      reverse_route.push_back(nfo->_address);
      nfo = _hosts.findp(nfo->_prev_to_me);
    }
    if (nfo && nfo->_metric_to_me == 0) {
      reverse_route.push_back(nfo->_address);
    }
  }
  if (from_me) {
	  Path route;
	  /* why isn't there just push? */
	  for (int i=reverse_route.size() - 1; i >= 0; i--) {
		  route.push_back(reverse_route[i]);
	  }
	  return route;
  }
  return reverse_route;
}

String
LinkTable::route_to_string(Path p) {
	StringAccum sa;
	int hops = p.size()-1;
	int metric = 0;
	StringAccum sa2;
	for (int i = 0; i < p.size(); i++) {
		sa2 << p[i];
		if (i != p.size()-1) {
			int m = get_link_metric(p[i], p[i+1]);
			sa2 << " (" << m << ") ";
			metric += m;
		}
	}
	sa << p[p.size()-1] << " hops " << hops << " metric " << metric << " " << sa2;
	return sa.take_string();
}

String
LinkTable::print_routes(bool from_me, bool pretty)
{
  StringAccum sa;

  Vector<IPAddress> addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++)
    addrs.push_back(iter.key());

  click_qsort(addrs.begin(), addrs.size(), sizeof(IPAddress), addr_sorter<IPAddress>);

  for (int x = 0; x < addrs.size(); x++) {
    IPAddress address = addrs[x];
    Path r = best_route(address, from_me);
    if (valid_route(r)) {
	    if (pretty) {
		    sa << route_to_string(r) << "\n";
	    } else {
		    sa << r[r.size()-1] << "  ";
		    for (int a = 0; a < r.size(); a++) {
			    sa << r[a];
			    if (a < r.size() - 1) {
				    sa << " " << get_link_metric(r[a],r[a+1]);
				    sa << " (" << get_link_seq(r[a],r[a+1])
				       << "," << get_link_age(r[a],r[a+1])
				       << ") ";
			    }
		    }
		    sa << "\n";
	    }
    }
  }
  return sa.take_string();
}

EXPORT_ELEMENT(LinkTable)
CLICK_ENDDECLS
