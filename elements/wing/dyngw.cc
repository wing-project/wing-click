/*
 * dyngw.{cc,hh}
 * Roberto Riggio
 *
 * Copyright (c) 2011 CREATE-NET
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
#include "dyngw.hh"
#include <click/confparse.hh>
#include "winggatewayselector.hh"
CLICK_DECLS

#define PROCENTRY_ROUTE "/proc/net/route"

DynGW::DynGW()
{
}


DynGW::~DynGW()
{
}

int DynGW::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh,
			"DEVNAME", cpkM, cpString, &_dev_name,
			"DEBUG", 0, cpBool, &_debug, 
			cpEnd) < 0)
		return -1;

	return 0;

}

Vector<HNAInfo> DynGW::fetch_hnas() {

	Vector<HNAInfo> hnas;

	char buff[1024], iface[17];
	uint32_t gate_addr, dest_addr, netmask;
	unsigned int iflags;
	int num, metric, refcnt, use;
	bool found = false;

	FILE *fp = fopen(PROCENTRY_ROUTE, "r");

	if (!fp) {
		click_chatter("%{element} :: %s :: cannot read proc file %s errno %s", 
				this, 
				__func__,
				PROCENTRY_ROUTE, 
				strerror(errno));
	}

	rewind(fp);

	while (fgets(buff, 1023, fp)) {
		num = sscanf(buff, "%16s %128X %128X %X %d %d %d %128X \n", iface, &dest_addr, &gate_addr, &iflags, &refcnt, &use, &metric, &netmask);
		if (num < 8) {
			continue;
		}
		if ((iflags & 1) && (metric == 0) && (iface != _dev_name)) {
			if (_debug) {
				click_chatter("%{element} :: %s :: gateway via %s detected in routing table", 
						this, 
						__func__,
						iface);
			}
			found = true;
		}
	}

	fclose(fp);

	if (!found) {
		if (_debug) {
			click_chatter("%{element} :: %s :: no gateways detected", 
					this, 
					__func__,
					iface);
		}
	}

	return hnas;

}

enum {
	H_DEBUG
};

String DynGW::read_handler(Element *e, void *thunk) {
	DynGW *td = (DynGW *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int DynGW::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	DynGW *f = (DynGW *) e;
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

void DynGW::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DynGW)

