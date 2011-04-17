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

DynGW::DynGW() :
  _enabled(true)
{
}


DynGW::~DynGW()
{
}

int DynGW::configure(Vector<String> &conf, ErrorHandler *errh) {
	String allow;
	if (cp_va_kparse(conf, this, errh,
			"DEVNAME", cpkP+cpkM, cpString, &_dev_name,
			"IP", cpkP+cpkM, cpIPAddress, &_ip, 
			"MASK", cpkP+cpkM, cpIPAddress, &_netmask, 
			"ALLOW", 0, cpString, &allow, 
			cpEnd) < 0)
		return -1;

	Vector<String> args;
	cp_spacevec(allow, args);
	if (args.size() == 0) {
		_allow.set(AddressPair(), true);
		return 0;
	}
	for (int x = 0; x < args.size(); x++) {
		int iface;
		IPAddress addr;
		IPAddress mask;
		if (!cp_ip_prefix(args[x], &addr, &mask)) {
			return errh->error("error param %s: must be an ip prefix", args[x].c_str());
		}
		_allow.set(AddressPair(addr, mask), true);
	}
	return 0;
}

void DynGW::fetch_hnas(Vector<HNAInfo> *hnas) {

	hnas->clear();

	char buff[1024], iface[17];
	uint32_t gate_addr, dest_addr, netmask;
	unsigned int iflags;
	int num, metric, refcnt, use;

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
			AddressPair pair = AddressPair(dest_addr, netmask);
			if (_allow.find(pair) != _allow.end()) {
				hnas->push_back(HNAInfo(IPAddress(dest_addr), IPAddress(netmask), _ip));
			}
		}
	}

	fclose(fp);

}

enum {
	H_ENABLED
};

String DynGW::read_handler(Element *e, void *thunk) {
	DynGW *c = (DynGW *) e;
	switch ((intptr_t) (thunk)) {
	case H_ENABLED:
		return String(c->_enabled) + "\n";
	default:
		return "<error>\n";
	}
}

int DynGW::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	DynGW *td = (DynGW *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
		case H_ENABLED: {
			bool enabled;
			if (!cp_bool(s, &enabled))
				return errh->error("debug parameter must be boolean");
			td->_enabled = enabled;
			break;
		}
	}
	return 0;
}

void DynGW::add_handlers() {
	add_read_handler("debug", read_handler, H_ENABLED);
	add_write_handler("enabled", write_handler, H_ENABLED);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DynGW)

