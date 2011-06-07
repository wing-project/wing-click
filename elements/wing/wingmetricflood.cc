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
#include <click/args.hh>
CLICK_DECLS

WINGMetricFlood::WINGMetricFlood() :
	WINGBase<QueryInfo>() {
	_seq = Timestamp::now().usec();
}

WINGMetricFlood::~WINGMetricFlood() {
}

int WINGMetricFlood::configure(Vector<String> &conf, ErrorHandler *errh) {

	return Args(conf, this, errh)
		.read_m("IP", _ip)
		.read_m("LT", ElementCastArg("LinkTableMulti"), _link_table)
		.read_m("ARP", ElementCastArg("ARPTableMulti"), _arp_table)
		.read("DEBUG", _debug)
		.complete();

}

void WINGMetricFlood::forward_seen(int iface, Seen *s) {
	PathMulti best = _link_table->best_route(s->_seen._src, false);
	if (_debug) {
		click_chatter("%{element} :: %s :: query %s seq %u iface %u", 
				this,
				__func__, 
				s->_seen._dst.unparse().c_str(), 
				s->_seq,
				iface);
	}
	Packet * p = create_wing_packet(NodeAddress(_ip, iface),
				NodeAddress(),
				WING_PT_QUERY, 
				s->_seen._dst, 
				NodeAddress(), 
				s->_seen._src, 
				s->_seq, 
				best,
				0);
	if (!p) {
		return;
	}
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
	QueryInfo query = QueryInfo(pk->qsrc(), pk->qdst());
	uint32_t seq = pk->seq();
	if (_debug) {
		click_chatter("%{element} :: %s :: forwarding query %s seq %d", 
				this, 
				__func__,
				query.unparse().c_str(), 
				seq);
	}

	/* update the metrics from the packet */
	if (!_link_table->update_link_table(p_in)) {
		p_in->kill();
		return;
	}

	if (pk->qdst() == _ip) {

		/* don't forward queries for me */

		if (process_seen(query, seq, false)) {

			PathMulti best = _link_table->best_route(pk->qsrc(), false);
			if (!_link_table->valid_route(best)) {
				click_chatter("%{element} :: %s :: invalid route %s", 
						this,
						__func__, 
						route_to_string(best).c_str());
				p_in->kill();
				return;
			}
			if (_debug) {
				click_chatter("%{element} :: %s :: generating reply %s seq %d", 
						this, 
						__func__,
						query.unparse().c_str(), 
						seq);
			}
			/* start reply */
			start_reply(best, seq);
			p_in->kill();
			return;

		}

	}

	/* process query */
	process_seen(query, seq, true);
	p_in->kill();
	return;
}

void WINGMetricFlood::start_reply(PathMulti best, uint32_t seq) {

	int hops = best.size() - 1;
	NodeAddress src = best[hops].arr();
	NodeAddress dst = best[hops - 1].dep();

	if (_debug) {
		click_chatter("%{element} :: %s :: starting reply %s < %s seq %u next %u (%s)", 
				this,
				__func__, 
				best[0].dep().unparse().c_str(),
				best[hops].arr()._ip.unparse().c_str(),
				seq,
				hops - 1,
				route_to_string(best).c_str());
	}

	Packet * p = create_wing_packet(src, 
			dst, 
			WING_PT_REPLY, 
			NodeAddress(), 
			NodeAddress(), 
			NodeAddress(), 
			seq, 
			best,
			hops - 1);

	if (!p) {
		return;
	}

	output(0).push(p);

}

void WINGMetricFlood::start_query(IPAddress dst, int iface) {
        QueryInfo query = QueryInfo(dst, _ip);
	if (_debug) {
		click_chatter("%{element} :: %s :: start query %s seq %d iface %u", 
				this, 
				__func__,
				query.unparse().c_str(), 
				_seq,
				iface);
	}
	Packet * p = create_wing_packet(NodeAddress(_ip, iface), 
				NodeAddress(), 
				WING_PT_QUERY, 
				query._dst, 
				NodeAddress(), 
				query._src, 
				_seq, 
				PathMulti(),
				0);
	if (!p) {
		return;
	}
	append_seen(query, _seq);
	output(0).push(p);
}

void WINGMetricFlood::start_flood(Packet *p_in) {
	IPAddress dst = p_in->dst_ip_anno();
	p_in->kill();
	Vector<int> ifs = _link_table->get_local_interfaces();
	for (int i = 0; i < ifs.size(); i++) {
		start_query(dst, ifs[i]);
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
	H_CLEAR_SEEN, 
	H_FLOODS
};

String WINGMetricFlood::read_handler(Element *e, void *thunk) {
	WINGMetricFlood *td = (WINGMetricFlood *) e;
	switch ((uintptr_t) thunk) {
		case H_FLOODS: {
			StringAccum sa;
			int x;
			for (x = 0; x < td->_seen.size(); x++) {
				sa << "src " << td->_seen[x]._seen.unparse();
				sa << " seq " << td->_seen[x]._seq;
				sa << " count " << td->_seen[x]._count;
				sa << " forwarded " << td->_seen[x]._forwarded;
				sa << "\n";
			}
			return sa.take_string();
		}
		default: {
			return WINGBase<QueryInfo>::read_handler(e, thunk);
		}
	}
}

int WINGMetricFlood::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *) {
	WINGMetricFlood *f = (WINGMetricFlood *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
		case H_CLEAR_SEEN: {
			f->_seen.clear();
			break;
		}
	}
	return 0;
}

void WINGMetricFlood::add_handlers() {
	WINGBase<QueryInfo>::add_handlers();
	add_read_handler("floods", read_handler, (void *) H_FLOODS);
	add_write_handler("clear", write_handler, (void *) H_CLEAR_SEEN);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(WINGBase LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGMetricFlood)
