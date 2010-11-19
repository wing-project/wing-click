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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
CLICK_DECLS

WINGSetGateway::WINGSetGateway() :
	_gw_sel(0) {
}

WINGSetGateway::~WINGSetGateway() {
}

int WINGSetGateway::configure(Vector<String> &conf, ErrorHandler *errh) {
	_gw = IPAddress();
	if (cp_va_kparse(conf, this, errh, 
				"GW", 0, cpIPAddress, &_gw, 
				"SEL", 0, cpElement, &_gw_sel, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	if (_gw_sel && _gw_sel->cast("WINGGatewaySelector") == 0)
		return errh->error("GW element is not a WINGGatewaySelector");
	if (!_gw_sel && !_gw) {
		return errh->error("Either GW or SEL must be specified!\n");
	}
	return 0;
}

int WINGSetGateway::initialize(ErrorHandler *) {
	return 0;
}

void WINGSetGateway::push(int, Packet *p_in) {
	if (_gw) {
		p_in->set_dst_ip_anno(_gw);
		output(0).push(p_in);
		return;
	} 
	IPAddress dst = p_in->dst_ip_anno();
	p_in->set_dst_ip_anno(_gw_sel->best_gateway(dst));
	output(0).push(p_in);
}

enum {
	H_GATEWAY, H_DEBUG
};

String WINGSetGateway::read_handler(Element *e, void *thunk) {
IPAddress tmp;
	WINGSetGateway *c = (WINGSetGateway *) e;
	switch ((intptr_t) (thunk)) {
	case H_GATEWAY:
		return (c->_gw) ? c->_gw.unparse() + "\n"
				: c->_gw_sel->best_gateway().unparse() + "\n";
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
	add_read_handler("debug", read_handler, H_DEBUG);
	add_read_handler("gateway", read_handler, H_GATEWAY);
	add_write_handler("debug", write_handler, H_DEBUG);
	add_write_handler("gateway", write_handler, H_GATEWAY);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(WINGGatewaySelector)
EXPORT_ELEMENT(WINGSetGateway)
