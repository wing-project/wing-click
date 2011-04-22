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
  _period(5000), _enabled(true), _timer(this)
{
}

DynGW::~DynGW()
{
}

int DynGW::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh,
			"DEVNAME", cpkM, cpString, &_dev_name,
			"SEL", cpkM, cpElementCast, "WINGGatewaySelector", &_sel,
			"PERIOD", 0, cpUnsigned, &_period, 
			"ENABLED", 0, cpBool, &_enabled, 
			cpEnd) < 0)
		return -1;

	return 0;

}

int DynGW::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void DynGW::run_timer(Timer *) {

	if (_enabled) {

		// clear current HNAs
		_sel->hnas_clear();

		// setup new HNAs
		char buff[1024], iface[17];
		uint32_t gate_addr, dest_addr, netmask;
		unsigned int iflags;
		int num, metric, refcnt, use;
		bool route_found = false;
		bool mesh_found = false;

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
			if ((iflags & 1) && (metric == 0)) {
				IPAddress addr = IPAddress(dest_addr);
				IPAddress mask = IPAddress(netmask);
				if ((addr == IPAddress()) && (mask == IPAddress())) {
					if (iface == _dev_name) {
						mesh_found = true;
					} else {
						route_found = true;
						_sel->hna_add(addr, mask);
					}
				}
			}
		}

		if ((mesh_found && route_found) || (!mesh_found && !route_found)) {
			String cmd;
			if (mesh_found && route_found) {
				// del default route
				cmd = "/sbin/route -n del default dev " + _dev_name;
			} else {
				// route add route
				cmd = "/sbin/route -n add default dev " + _dev_name;
			}
			if (system(cmd.c_str()) != 0) {
				click_chatter("%{element} :: %s :: unable to execute command \"%s\", errno %s", 
						this, 
						__func__,
						cmd.c_str(), 
						strerror(errno));
			} else {
				click_chatter("%{element} :: %s :: %s", 
						this, 
						__func__,
						cmd.c_str());
			}
		}

		fclose(fp);

	}

	// schedule next timer
	_timer.schedule_at(Timestamp::now() + Timestamp::make_msec(_period));

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

