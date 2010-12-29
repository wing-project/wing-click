/*
 * devinfo.{cc,hh}
 * Stefano Testi, Roberto Riggio
 *
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
#include "devinfo.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <elements/wifi/availablerates.hh>
#include "availablechannels.hh"
CLICK_DECLS

DevInfo::DevInfo() 
{
}

DevInfo::~DevInfo() {
}

int DevInfo::configure(Vector<String> &conf, ErrorHandler *errh) {
	_debug = false;

	if (cp_va_kparse(conf, this, errh,
			"ETH", cpkM, cpEthernetAddress, &_eth,
			"IFACE", cpkM, cpUnsigned, &_iface,
			"CHANNEL", cpkM, cpUnsigned, &_channel,
			"CHANNELS", cpkM, cpElementCast, "AvailableChannels", &_ctable,
			"RATES", cpkM, cpElementCast, "AvailableRates", &_rtable,
			"DEBUG", 0, cpBool, &_debug,
			cpEnd) < 0)
		return -1;
	
	return 0;
}

enum {
	H_DEBUG, H_CHANNEL
};

String DevInfo::read_handler(Element *e, void *thunk) {
	DevInfo *f = (DevInfo *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(f->_debug) + "\n";
	case H_CHANNEL:
		return String(f->channel()) + "\n";
	default:
		return String();
	}
}

int DevInfo::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	DevInfo *f = (DevInfo *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void DevInfo::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("channel", read_handler, (void *) H_CHANNEL);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DevInfo)
