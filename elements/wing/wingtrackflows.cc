/*
 * wingtrackflows.{cc,hh} 
 * John Bicket, Alexander Yip, Roberto Riggio, Stefano Testi
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
#include "wingtrackflows.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
CLICK_DECLS

WINGTrackFlows::WINGTrackFlows() :
	_timer(this), _period(60000) {
}

WINGTrackFlows::~WINGTrackFlows() {
}

int WINGTrackFlows::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (cp_va_kparse(conf, this, errh, 
				"PERIOD", 0, cpUnsigned, &_period, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;
	return 0;
}

int WINGTrackFlows::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void WINGTrackFlows::run_timer(Timer *) {
	cleanup();
	_timer.schedule_after_msec(_period);
}

void WINGTrackFlows::push_fwd(Packet *p_in) {
	const click_tcp *tcph = p_in->tcp_header();
	IPFlowID flowid = IPFlowID(p_in);
	FlowTableEntry *match = _flow_table.findp(flowid);
	if ((tcph->th_flags & TH_SYN) && match && match->is_pending()) {
		match->_outstanding_syns++;
		p_in->set_dst_ip_anno(match->_gw);
		output(0).push(p_in);
		return;
	} else if (!(tcph->th_flags & TH_SYN)) {
		if (match) {
			match->saw_forward_packet();
			if (tcph->th_flags & (TH_RST | TH_FIN)) {
				match->_fwd_alive = false; // forward flow is over
			}
			if (tcph->th_flags & TH_RST) {
				match->_rev_alive = false; // rev flow is over
			}
			p_in->set_dst_ip_anno(match->_gw);
			output(0).push(p_in);
			return;
		}
		click_chatter("%{element} :: %s :: no match, guessing for %s", 
				this,
				__func__, 
				flowid.unparse().c_str());
	}
	/* no match */
	IPAddress gw = p_in->dst_ip_anno();
	if (!gw) {
		click_chatter("%{element} :: %s :: unable to set gateway for dst %s", 
				this,
				__func__, 
				p_in->dst_ip_anno().unparse().c_str());
		p_in->kill();
		return;
	}
	_flow_table.insert(flowid, FlowTableEntry());
	match = _flow_table.findp(flowid);
	match->_id = flowid;
	match->_gw = gw;
	match->saw_forward_packet();
	match->_outstanding_syns++;
	p_in->set_dst_ip_anno(gw);
	output(0).push(p_in);
}

void WINGTrackFlows::push_rev(Packet *p_in) {
	const click_tcp *tcph = p_in->tcp_header();
	IPFlowID flowid = IPFlowID(p_in).reverse();
	FlowTableEntry *match = _flow_table.findp(flowid);
	if ((tcph->th_flags & TH_SYN) && (tcph->th_flags & TH_ACK)) {
		if (match) {
			if (match->_gw != MISC_IP_ANNO(p_in)) {
				click_chatter("%{element} :: %s :: flow %s got packet from weird gw %s, expected %s",
						this, 
						__func__, flowid.unparse().c_str(),
						p_in->dst_ip_anno().unparse().c_str(),
						match->_gw.unparse().c_str());
				p_in->kill();
				return;
			}
			match->saw_reply_packet();
			match->_outstanding_syns = 0;
			output(1).push(p_in);
			return;
		}
		click_chatter("%{element} :: %s :: no match, killing SYN_ACK", this, __func__);
		p_in->kill();
		return;
	}
	/* not a syn-ack packet */
	if (match) {
		match->saw_reply_packet();
		if (tcph->th_flags & (TH_FIN | TH_RST)) {
			match->_rev_alive = false;
		}
		if (tcph->th_flags & TH_RST) {
			match->_fwd_alive = false;
		}
		output(1).push(p_in);
		return;
	}
	click_chatter("%{element} :: %s :: couldn't find non-pending match, creating %s",
			this, 
			__func__, 
			flowid.unparse().c_str());
	_flow_table.insert(flowid, FlowTableEntry());
	match = _flow_table.findp(flowid);
	match->_id = flowid;
	match->_gw = MISC_IP_ANNO(p_in);
	match->saw_reply_packet();
	output(1).push(p_in);
	return;
}

void WINGTrackFlows::push(int port, Packet *p_in) {
	/* non tcp packets */
	if (p_in->ip_header()->ip_p != IP_PROTO_TCP) {
		output(port).push(p_in);
		return;
	}
	if (port == 0) {
		/* outgoing packet */
		push_fwd(p_in);
	} else {
		/* incoming packet */
		push_rev(p_in);
	}
}

void WINGTrackFlows::cleanup() {
	FlowTable new_table;
	Timestamp timeout = Timestamp::make_msec(_period);
	for (FTIter i = _flow_table.begin(); i.live(); i++) {
		FlowTableEntry f = i.value();
		if ((f.age() < timeout && ((f._fwd_alive) || f._rev_alive))) {
			new_table.insert(f._id, f);
		}
	}
	_flow_table.clear();
	for (FTIter i = new_table.begin(); i.live(); i++) {
		FlowTableEntry f = i.value();
		_flow_table.insert(f._id, f);
	}
}

String WINGTrackFlows::print_flows() {
	StringAccum sa;
	for (FTIter iter = _flow_table.begin(); iter.live(); iter++) {
		FlowTableEntry f = iter.value();
		sa << f._id << " gw " << f._gw << " age " << f.age() << "\n";
	}
	return sa.take_string();
}

enum {
	H_FLOWS, H_DEBUG
};

String WINGTrackFlows::read_handler(Element *e, void *thunk) {
	WINGTrackFlows *c = (WINGTrackFlows *) e;
	switch ((intptr_t) (thunk)) {
	case H_DEBUG:
		return String(c->_debug) + "\n";
	case H_FLOWS:
		return c->print_flows();
	default:
		return "<error>\n";
	}
}

int WINGTrackFlows::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	WINGTrackFlows *d = (WINGTrackFlows *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		d->_debug = debug;
		break;
	}
	}
	return 0;
}

void WINGTrackFlows::add_handlers() {
	add_read_handler("flows", read_handler, H_FLOWS);
	add_read_handler("debug", read_handler, H_DEBUG);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WINGTrackFlows)
