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

int
LinkTable::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  int stale_period = 120;

  ret = Args(conf, this, errh)
      .read("IP", _ip)
      .read("STALE", stale_period)
      .complete();

  _stale_timeout.assign(stale_period, 0);
  _hosts.insert(_ip, HostInfo(_ip));
  return ret;
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

void
LinkTable::dijkstra(bool from_me)
{

  typedef HashMap<IPAddress, bool> AddressMap;
  typedef AddressMap::const_iterator AMIter;

  AddressMap addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    addrs.insert(iter.value()._address, true);
  }

  for (AMIter i = addrs.begin(); i.live(); i++) {
    /* clear them all initially */
    HostInfo *n = _hosts.findp(i.key());
    n->clear(from_me);
  }
  HostInfo *root_info = _hosts.findp(_ip);

  assert(root_info);

  if (from_me) {
    root_info->_prev_from_me = root_info->_address;
    root_info->_metric_from_me = 0;
  } else {
    root_info->_prev_to_me = root_info->_address;
    root_info->_metric_to_me = 0;
  }

  IPAddress current_min_address = root_info->_address;

  while (current_min_address) {
    HostInfo *current_min = _hosts.findp(current_min_address);
    assert(current_min);
    if (from_me) {
      current_min->_marked_from_me = true;
    } else {
      current_min->_marked_to_me = true;
    }


    for (AMIter i = addrs.begin(); i.live(); i++) {
      HostInfo *neighbor = _hosts.findp(i.key());
      assert(neighbor);
      bool marked = neighbor->_marked_to_me;
      if (from_me) {
	marked = neighbor->_marked_from_me;
      }

      if (marked) {
	continue;
      }

      AddressPair pair = AddressPair(neighbor->_address, current_min_address);
      if (from_me) {
	pair = AddressPair(current_min_address, neighbor->_address);
      }
      LinkInfo *lnfo = _links.findp(pair);
      if (!lnfo || !lnfo->_metric) {
	continue;
      }
      uint32_t neighbor_metric = neighbor->_metric_to_me;
      uint32_t current_metric = current_min->_metric_to_me;

      if (from_me) {
	neighbor_metric = neighbor->_metric_from_me;
	current_metric = current_min->_metric_from_me;
      }


      uint32_t adjusted_metric = current_metric + lnfo->_metric;
      if (!neighbor_metric ||
	  adjusted_metric < neighbor_metric) {
	if (from_me) {
	  neighbor->_metric_from_me = adjusted_metric;
	  neighbor->_prev_from_me = current_min_address;
	} else {
	  neighbor->_metric_to_me = adjusted_metric;
	  neighbor->_prev_to_me = current_min_address;
	}

      }
    }

    current_min_address = IPAddress();
    uint32_t  min_metric = ~0;
    for (AMIter i = addrs.begin(); i.live(); i++) {
      HostInfo *nfo = _hosts.findp(i.key());
      uint32_t metric = nfo->_metric_to_me;
      bool marked = nfo->_marked_to_me;
      if (from_me) {
	metric = nfo->_metric_from_me;
	marked = nfo->_marked_from_me;
      }
      if (!marked && metric &&
	  metric < min_metric) {
        current_min_address = nfo->_address;
        min_metric = metric;
      }
    }

  }
}

EXPORT_ELEMENT(LinkTable)
CLICK_ENDDECLS
