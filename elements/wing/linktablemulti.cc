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
#include <click/args.hh>
CLICK_DECLS

bool
cp_node_address(String s, NodeAddress *node)
{
	int sep = s.find_left(':');
	IPAddress ip;
	int iface;
	if (!cp_ip_address(s.substring(0, sep), &ip) ||
		!cp_integer(s.substring(sep + 1, s.length()), &iface)) {
		return false;
	}
	*node = NodeAddress(ip, iface);
	return true;
}

LinkTableMulti::LinkTableMulti() : _beta(20)
{
}

LinkTableMulti::~LinkTableMulti()
{
}

void *
LinkTableMulti::cast(const char *n) {
    if (strcmp(n, "LinkTableMulti") == 0)
        return (LinkTableMulti *) this;
    else if (strcmp(n, "LinkTableBase") == 0)
        return (LinkTableBase<NodeAddress, PathMulti> *) this;
    else
        return 0;
}

int 
LinkTableMulti::configure(Vector<String> &conf, ErrorHandler *errh) {
    int stale_period = 120;
    IPAddress ip;
    String ifaces;

    if (Args(conf, this, errh)
          .read_m("IP", ip)
          .read_m("IFACES", ifaces)
          .read("BETA", _beta)
          .read("STALE", stale_period)
          .read("DEBUG", _debug)
          .complete())
      return -1;

    _ip = NodeAddress(ip, 0);
    _hosts.insert(_ip, HostInfo(_ip));

    if (_beta > 100) {
        return errh->error("Error param BETA must be > 100, given %u", _beta);
    }

    Vector<String> args;
    cp_spacevec(ifaces, args);
    if (args.size() < 1) {
        return errh->error("Error param IFACES must have > 0 arg, given %u", args.size());
    }
    for (int x = 0; x < args.size(); x++) {
        int iface;
        if (!cp_unsigned(args[x], &iface)) {
            errh->error("argument %s should be an unsigned", args[x].c_str());
        }
        NodeAddress node = NodeAddress(ip, iface);
        _hosts.insert(node, HostInfo(node));
    }
    _stale_timeout.assign(stale_period, 0);
    return 0;
}

bool 
LinkTableMulti::update_link_table(Packet *p) {
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	/* update the metrics from the packet */
	for (int i = 0; i < pk->num_links(); i++) {
		NodeAddress a = pk->get_link_dep(i);
		NodeAddress b = pk->get_link_arr(i);
		uint32_t fwd_m = pk->get_link_fwd(i);
		uint32_t rev_m = pk->get_link_rev(i);
		uint32_t seq = pk->get_link_seq(i);
		uint32_t age = pk->get_link_age(i);
		uint16_t channel = pk->get_link_channel(i);
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
			return false;
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					seq,
					age,
					fwd_m,
					channel,
					b.unparse().c_str());
		}
		if (fwd_m && !update_link(a, b, seq, age, fwd_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update fwd_m %s > %d > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					b.unparse().c_str());
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					b.unparse().c_str(), 
					seq,
					age,
					rev_m,
					channel,
					a.unparse().c_str());
		}
		if (rev_m && !update_link(b, a, seq, age, rev_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update rev_m %s < %d < %s",
					this, 
					__func__, 
					b.unparse().c_str(), 	
					rev_m,
					a.unparse().c_str());
		}
	}
	dijkstra(true);
	dijkstra(false);
	return true;
}

PathMulti 
LinkTableMulti::best_route(NodeAddress dst, bool from_me)
{
    PathMulti reverse_route;
    if (!dst) {
        return reverse_route;
    }
    HostInfo *nfo = _hosts.findp(dst);
    if (from_me) {
        Vector<NodeAddress> raw_path;
        while (nfo && nfo->_metric_from_me != 0) {
            if (nfo->_address._iface != 0) {
                raw_path.push_back(nfo->_address);
            }
            nfo = _hosts.findp(nfo->_prev_from_me);
        }
        if (raw_path.size() < 1) {
            return reverse_route;
        }
        reverse_route.push_back(NodeAirport(dst, raw_path[0]._iface, 0));
        for (int x = 1; x < raw_path.size() - 1; x++) {
            if (raw_path[x]._ip == raw_path[x + 1]._ip) {
                reverse_route.push_back(NodeAirport(raw_path[x]._ip, raw_path[x + 1]._iface, raw_path[x]._iface));
                x++;
            } else {
                reverse_route.push_back(NodeAirport(raw_path[x]._ip, raw_path[x]._iface, raw_path[x]._iface));
            }
        }
        reverse_route.push_back(NodeAirport(_ip, 0, raw_path[raw_path.size() - 1]._iface));
    } else {
        Vector<NodeAddress> raw_path;
        while (nfo && nfo->_metric_to_me != 0) {
            if (nfo->_address._iface != 0) {
                raw_path.push_back(nfo->_address);
            }
            nfo = _hosts.findp(nfo->_prev_to_me);
        }
        if (raw_path.size() < 1) {
            return reverse_route;
        }
        reverse_route.push_back(NodeAirport(dst, 0, raw_path[0]._iface));
        for (int x = 1; x < raw_path.size() - 1; x++) {
            if (raw_path[x]._ip == raw_path[x + 1]._ip) {
                reverse_route.push_back(NodeAirport(raw_path[x]._ip, raw_path[x]._iface,  raw_path[x + 1]._iface));
                x++;
            } else {
                reverse_route.push_back(NodeAirport(raw_path[x]._ip, raw_path[x]._iface,  raw_path[x]._iface));
            }
        }
        reverse_route.push_back(NodeAirport(_ip, raw_path[raw_path.size() - 1]._iface, 0));
    }
    if (from_me) {
        Vector<NodeAirport> route;
        for (int i = reverse_route.size() - 1; i >= 0; i--) {
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
    sa << p[p.size()-1]._ip.unparse() << " hops " << hops << " metric " << metric << " " << sa2;
    return sa.take_string();
}

String 
LinkTableMulti::print_routes(bool from_me, bool pretty)
{
    StringAccum sa;
    Vector<NodeAddress> addrs;
    for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
        if (iter.key()._iface == 0) {
            addrs.push_back(iter.key());
        }
    }
    click_qsort(addrs.begin(), addrs.size(), sizeof(NodeAddress), addr_sorter<NodeAddress>);
    for (int x = 0; x < addrs.size(); x++) {
        NodeAddress address = addrs[x];
        PathMulti r = best_route(address, from_me);
        if (valid_route(r)) {
            if (pretty) {
                sa << route_to_string(r) << "\n";
            } else {
                sa << r[r.size()-1] << "  ";
                for (int a = 0; a < r.size(); a++) {
                    sa << r[a].unparse();
                    if (a != r.size()-1) {
                        NodeAddress nfrom = NodeAddress(r[a]._ip, r[a]._dep);
                        NodeAddress nto = NodeAddress(r[a+1]._ip, r[a+1]._arr);
                        sa << " " << get_link_metric(nfrom,nto);
                        sa << " (" << get_link_seq(nfrom,nto) << "," << get_link_age(nfrom,nto) << ") ";
                    }
                }
                sa << "\n";
            }
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

uint32_t
LinkTableMulti::compute_metric(Vector<uint32_t> metrics, Vector<uint32_t> channels) {
    if (metrics.size() != channels.size()) {
          return 99999;
    }
    HashMap<uint32_t, uint32_t> mt;
    uint32_t max = 0;
    uint32_t ett = 0;
    for (int x = 0; x < metrics.size(); x++) {
        uint32_t *nc = mt.findp(channels[x]);
       if (!nc) {
           mt.insert(channels[x], 0);
           nc = mt.findp(channels[x]);
       }
       *nc += metrics[x];
       ett += metrics[x];
    }
    for (HashMap<uint32_t, uint32_t>::const_iterator iter = mt.begin(); iter.live(); iter++) {
        if (iter.value() > max) {
           max=iter.value();
        }
    }
    return (ett * (100 - _beta) + max * _beta) / 100;
}

void
LinkTableMulti::dijkstra(bool from_me)
{

  Timestamp start = Timestamp::now();

  typedef HashMap<NodeAddress, bool> AddressMap;
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

  NodeAddress current_min_address = root_info->_address;

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

      if (!lnfo || !lnfo->_metric || !lnfo->_channel) {
        continue;
      }

      Vector<uint32_t> metrics;
      Vector<uint32_t> channels;

      HostInfo * thi = current_min;

      if (from_me) {
          while (thi->_prev_from_me != thi->_address) {
              AddressPair prev = AddressPair(thi->_prev_from_me, thi->_address);
              LinkInfo *tli = _links.findp(prev);
              if (tli) {
                  metrics.push_back(tli->_metric);
                  channels.push_back(tli->_channel);
              }
              thi = _hosts.findp(thi->_prev_from_me);
          }
      } else {
          while (thi->_prev_to_me != thi->_address) {
              AddressPair prev = AddressPair(thi->_address, thi->_prev_to_me);
              LinkInfo *tli = _links.findp(prev);
              if (tli) {
                  metrics.push_back(tli->_metric);
                  channels.push_back(tli->_channel);
              }
              thi = _hosts.findp(thi->_prev_to_me);
          }
      }

      metrics.push_back(lnfo->_metric);
      channels.push_back(lnfo->_channel);

      uint32_t neighbor_metric = neighbor->_metric_to_me;
      if (from_me) {
        neighbor_metric = neighbor->_metric_from_me;
      }

      uint32_t adjusted_metric = compute_metric(metrics, channels);

      if (!neighbor_metric || adjusted_metric < neighbor_metric) {
        if (from_me) {
          neighbor->_metric_from_me = adjusted_metric;
          neighbor->_prev_from_me = current_min_address;
        } else {
          neighbor->_metric_to_me = adjusted_metric;
          neighbor->_prev_to_me = current_min_address;
        }
      }
    }

    current_min_address = NodeAddress();
    uint32_t  min_metric = ~0;
    for (AMIter i = addrs.begin(); i.live(); i++) {
      HostInfo *nfo = _hosts.findp(i.key());
      uint32_t metric = nfo->_metric_to_me;
      bool marked = nfo->_marked_to_me;
      if (from_me) {
        metric = nfo->_metric_from_me;
        marked = nfo->_marked_from_me;
      }
      if (!marked && metric && metric < min_metric) {
        current_min_address = nfo->_address;
        min_metric = metric;
      }
    }
  }

  _dijkstra_time = Timestamp::now() - start;

}

enum { H_HOST_INTERFACES };

String 
LinkTableMulti::read_handler(Element *e, void *thunk) {
  LinkTableMulti *td = (LinkTableMulti *) e;
  switch ((uintptr_t) thunk) {
    case H_HOST_INTERFACES: {
      Vector<int> ifaces = td->get_local_interfaces();
      StringAccum sa;
      for (int x = 0; x < ifaces.size(); x++) {
        sa << ifaces[x] << "\n";
      }
      return sa.take_string();
    }
    default:
      return LinkTableBase<NodeAddress, PathMulti>::read_handler(e, thunk);
    }
}

int 
LinkTableMulti::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler * errh) {
  LinkTableMulti *f = (LinkTableMulti *) e;
  String s = cp_uncomment(in_s);
  switch ((intptr_t) vparam) {
    case H_UPDATE_LINK: {
      Vector<String> args;
      NodeAddress from;
      NodeAddress to;
      uint32_t seq;
      uint32_t age;
      uint32_t metric;
      uint32_t channel;
      cp_spacevec(in_s, args);
      if (args.size() != 6) {
        return errh->error("Must have six arguments: currently has %d: %s", args.size(), args[0].c_str());
      }
      if (!cp_node_address(args[0], &from)) {
        return errh->error("Couldn't read NodeAddress out of from");
      }
      if (!cp_node_address(args[1], &to)) {
        return errh->error("Couldn't read NodeAddress out of to");
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
    default: {
      return LinkTableBase<NodeAddress, PathMulti>::write_handler(in_s, e, vparam, errh);
    }
  }
  return 0;
}

void
LinkTableMulti::add_handlers() {
    LinkTableBase<NodeAddress, PathMulti>::add_handlers();
    add_read_handler("interfaces", read_handler, H_HOST_INTERFACES);
    add_write_handler("update_link", write_handler, H_UPDATE_LINK);
}

EXPORT_ELEMENT(LinkTableMulti)
CLICK_ENDDECLS
