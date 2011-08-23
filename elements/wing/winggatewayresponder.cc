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
#include <click/args.hh>
#include "winggatewayselector.hh"
#include "wingmetricflood.hh"
CLICK_DECLS

WINGGatewayResponder::WINGGatewayResponder() :
	_timer(this) {
}

WINGGatewayResponder::~WINGGatewayResponder() {
}

int WINGGatewayResponder::configure(Vector<String> &conf, ErrorHandler *errh) {

	return Args(conf, this, errh)
		.read_m("LT", ElementCastArg("LinkTableMulti"), _link_table)
		.read_m("SEL", ElementCastArg("WINGGatewaySelector"), _gw_sel)
		.read_m("MF", ElementCastArg("WINGMetricFlood"), _metric_flood)
		.read("PERIOD", _period)
		.complete();

}

int WINGGatewayResponder::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void WINGGatewayResponder::run_timer(Timer *) {
	if (!_gw_sel->is_gateway()) {
		PathMulti best = _link_table->best_route(_gw_sel->best_gateway(), false);
		if (_link_table->valid_route(best)) {
			_metric_flood->start_reply(best, 0);
		}
	}
	unsigned max_jitter = _period / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	_timer.schedule_after_msec(_period + j - max_jitter);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGGatewayResponder)
