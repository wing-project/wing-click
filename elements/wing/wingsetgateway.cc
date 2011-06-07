/*
 * wingsetgateway.{cc,hh} 
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
#include "wingsetgateway.hh"
#include <click/args.hh>
#include "winggatewayselector.hh"
CLICK_DECLS

WINGSetGateway::WINGSetGateway() :
	_timer(this), _period(60000) {
}

WINGSetGateway::~WINGSetGateway() {
}

int WINGSetGateway::configure(Vector<String> &conf, ErrorHandler *errh) {

	int ret;

	ret = Args(conf, this, errh)
		.read("GW", _gw)
		.read("SEL", ElementCastArg("WINGGatewaySelector"), _gw_sel)
		.read("PERIOD", _period)
		.read("DEBUG", _debug)
		.complete();

	if (!_gw_sel && !_gw) {
		return errh->error("Either GW or SEL must be specified!\n");
	}

	return ret;
}

int WINGSetGateway::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void WINGSetGateway::run_timer(Timer *) {
	// clean up flows
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
	// re-schedule timer
	_timer.schedule_after_msec(_period);
}

void WINGSetGateway::push_fwd(Packet *p_in) {
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
	IPAddress dst = p_in->dst_ip_anno();
	if (!dst) {	
		click_chatter("%{element} :: %s :: dst annotation not set %s", 
				this, 
				__func__,
				dst.unparse().c_str());
		p_in->kill();
		return;
	}
	IPAddress gw = _gw_sel->best_gateway(dst);
	if (!gw) {	
		click_chatter("%{element} :: %s :: unable to find gw for %s", 
				this, 
				__func__,
				dst.unparse().c_str());
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

void WINGSetGateway::push_rev(Packet *p_in) {
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

void WINGSetGateway::push(int port, Packet *p_in) {

	if (_gw) {
		if (port == 0) {
			p_in->set_dst_ip_anno(_gw);
		} else {
			p_in->set_dst_ip_anno(IPAddress());
		}
		output(port).push(p_in);
		return;
	}

	if (p_in->ip_header()->ip_p != IP_PROTO_TCP) {
		/* non tcp packets always go to best gw */
		if (port == 0) {
			IPAddress dst = p_in->dst_ip_anno();
			if (!dst) {	
				click_chatter("%{element} :: %s :: dst annotation not set %s", 
						this, 
						__func__,
						dst.unparse().c_str());
				p_in->kill();
				return;
			}
			IPAddress gw = _gw_sel->best_gateway(dst);
			if (!gw) {	
				click_chatter("%{element} :: %s :: unable to find gw for %s", 
						this, 
						__func__,
						dst.unparse().c_str());
				p_in->kill();
				return;
			}
			p_in->set_dst_ip_anno(gw);
		} else {
			p_in->set_dst_ip_anno(IPAddress());
		}
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

String WINGSetGateway::print_flows() {
	StringAccum sa;
	for (FTIter iter = _flow_table.begin(); iter.live(); iter++) {
		FlowTableEntry f = iter.value();
		sa << f._id << " gw " << f._gw << " age " << f.age() << "\n";
	}
	return sa.take_string();
}

enum {
	H_GATEWAY, H_FLOWS
};

String WINGSetGateway::read_handler(Element *e, void *thunk) {
	WINGSetGateway *c = (WINGSetGateway *) e;
	switch ((intptr_t) (thunk)) {
		case H_GATEWAY:
			return (c->_gw) ? c->_gw.unparse() + "\n"
					: c->_gw_sel->best_gateway().unparse() + "\n";
		case H_FLOWS:
			return c->print_flows();
		default:
			return "<error>\n";
	}
}

int WINGSetGateway::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	WINGSetGateway *d = (WINGSetGateway *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
		case H_GATEWAY: {
			IPAddress ip;
			if (!cp_ip_address(s, &ip)) {
				return errh->error("gateway parameter must be IPAddress");
			}
			if (!ip && !d->_gw_sel) {
				return errh->error("gateway cannot be %s if _gw_sel is unspecified");
			}
			d->_gw = ip;
			break;
		}
	}
	return 0;
}

void WINGSetGateway::add_handlers() {
	add_read_handler("flows", read_handler, H_FLOWS);
	add_read_handler("gateway", read_handler, H_GATEWAY);
	add_write_handler("gateway", write_handler, H_GATEWAY);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WINGSetGateway)
