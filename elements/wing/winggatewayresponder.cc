/*
 * winggatewayresponder.{cc,hh}
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
#include "winggatewayresponder.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "linktablemulti.hh"
#include "winggatewayselector.hh"
#include "winggatewayresponder.hh"
#include "wingqueryresponder.hh"
CLICK_DECLS

WINGGatewayResponder::WINGGatewayResponder() :
	_timer(this), _debug(false) {
}

WINGGatewayResponder::~WINGGatewayResponder() {
}

int WINGGatewayResponder::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
				"SEL", cpkM, cpElementCast, "WINGGatewaySelector", &_gw_sel, 
				"RESPONDER", cpkM, cpElementCast, "WINGQueryResponder", &_responder, 
				"PERIOD", 0, cpUnsigned, &_period, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	return 0;

}

int WINGGatewayResponder::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void WINGGatewayResponder::run_timer(Timer *) {
	if (!_gw_sel->is_gateway()) {
		IPAddress gateway = _gw_sel->best_gateway();
		PathMulti best = _link_table->best_route(gateway, false);
		if (_link_table->valid_route(best)) {
			_responder->start_reply(best, 0);
		}
	}
	unsigned max_jitter = _period / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	Timestamp delay = Timestamp::make_msec(_period + j - max_jitter);
	_timer.schedule_at(Timestamp::now() + delay);
}

enum {
	H_DEBUG
};

String WINGGatewayResponder::read_handler(Element *e, void *thunk) {
	WINGGatewayResponder *c = (WINGGatewayResponder *) e;
	switch ((intptr_t) (thunk)) {
	case H_DEBUG:
		return String(c->_debug) + "\n";
	default:
		return "<error>\n";
	}
}

int WINGGatewayResponder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {
	WINGGatewayResponder *d = (WINGGatewayResponder *) e;
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

void WINGGatewayResponder::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGGatewayResponder)
