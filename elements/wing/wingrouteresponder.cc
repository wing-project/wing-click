/*
 * wingrouteresponder.{cc,hh} -- Route error generator
 * Roberto Riggio
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2010 CREATE-NET
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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "wingrouteresponder.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
CLICK_DECLS

WINGRouteResponder::WINGRouteResponder() : _debug(false) {
}

WINGRouteResponder::~WINGRouteResponder() {
}

int WINGRouteResponder::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"ARP", cpkM, cpElementCast, "ARPTableMulti", &_arp_table, 
				"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	return 0;

}

void 
WINGRouteResponder::push(int, Packet *p_in) {
	click_ether *eh = (click_ether *) p_in->data();
	struct wing_data *pk = (struct wing_data *) (eh+1);
	if (pk->_type == WING_PT_DATA ) {
		/* do something cool */
	}
	if (noutputs() == 1) {
		output(0).push(p_in);
	} else {
		p_in->kill();
	}
}

enum {
	H_DEBUG
};

String WINGRouteResponder::read_handler(Element *e, void *thunk) {
	WINGRouteResponder *td = (WINGRouteResponder *) e;
	switch ((intptr_t) (thunk)) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return "<error>\n";
	}
}

int WINGRouteResponder::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	WINGRouteResponder *td = (WINGRouteResponder *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
		case H_DEBUG: {
			bool debug;
			if (!cp_bool(s, &debug))
				return errh->error("debug parameter must be boolean");
			td->_debug = debug;
			break;
		}
	}
	return 0;
}

void WINGRouteResponder::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGRouteResponder)
