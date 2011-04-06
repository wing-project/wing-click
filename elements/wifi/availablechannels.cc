/*
 * availablechannels.{cc,hh} 
 * John Bicket, Stefano Testi, Roberto Riggio
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "availablechannels.hh"
CLICK_DECLS

AvailableChannels::AvailableChannels() {
}

AvailableChannels::~AvailableChannels() {
}

int AvailableChannels::parse_and_insert(String s, ErrorHandler *errh) {
	EtherAddress e;
	Vector<int> channels;
	Vector<String> args;
	cp_spacevec(s, args);
	if (args.size() < 2) {
		return errh->error("error param %s must have > 1 arg", s.c_str());
	}
	bool default_channels = false;
	if (args[0] == "DEFAULT") {
		default_channels = true;
		_default_channels.clear();
	} else {
		if (!cp_ethernet_address(args[0], &e))
			return errh->error(
					"error param %s: must start with ethernet address",
					s.c_str());
	}

	for (int x = 1; x < args.size(); x++) {
		int r;
		cp_integer(args[x], &r);
		if (default_channels) {
			_default_channels.push_back(r);
		} else {
			channels.push_back(r);
		}
	}

	if (default_channels) {
		return 0;
	}

	DstInfo d = DstInfo(e);
	d._channels = channels;
	d._eth = e;
	_rtable.insert(e, d);
	return 0;
}

int AvailableChannels::configure(Vector<String> &conf, ErrorHandler *errh) {
	int res = 0;
	_debug = false;
	for (int x = 0; x < conf.size(); x++) {
		res = parse_and_insert(conf[x], errh);
		if (res != 0) {
			return res;
		}
	}
	return res;
}

void AvailableChannels::take_state(Element *e, ErrorHandler *) {
	AvailableChannels *q = (AvailableChannels *) e->cast("AvailableChannels");
	if (!q)
		return;
	_rtable = q->_rtable;
	_default_channels = _default_channels;
}

Vector<int> AvailableChannels::lookup(EtherAddress eth) {
	if (!eth) {
		if (_debug) {
			click_chatter("%{element} :: %s :: invalid address %s", 
					this, 
					__func__,
					eth.unparse().c_str());
		}
		return Vector<int> ();
	}

	DstInfo *dst = _rtable.findp(eth);
	if (dst) {
		return dst->_channels;
	}

	if (_default_channels.size()) {
		return _default_channels;
	}

	return Vector<int> ();
}

int AvailableChannels::insert(EtherAddress eth, Vector<int> channels) {
	if (!eth) {
		if (_debug) {
			click_chatter("%{element} :: %s :: invalid address %s", 
					this, 
					__func__,
					eth.unparse().c_str());
		}
		return -1;
	}
	DstInfo *dst = _rtable.findp(eth);
	if (!dst) {
		_rtable.insert(eth, DstInfo(eth));
		dst = _rtable.findp(eth);
	}
	dst->_eth = eth;
	dst->_channels.clear();
	if (_default_channels.size()) {
		/* only add channels that are in the default channels */
		for (int x = 0; x < channels.size(); x++) {
			for (int y = 0; y < _default_channels.size(); y++) {
				if (channels[x] == _default_channels[y]) {
					dst->_channels.push_back(channels[x]);
				}
			}
		}
	} else {
		dst->_channels = channels;
	}
	return 0;
}

enum {
	H_DEBUG, H_INSERT, H_REMOVE, H_CHANNELS
};

String AvailableChannels::read_handler(Element *e, void *thunk) {
	AvailableChannels *td = (AvailableChannels *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_CHANNELS: {
		StringAccum sa;
		if (td->_default_channels.size()) {
			sa << "DEFAULT ";
			for (int x = 0; x < td->_default_channels.size(); x++) {
				sa << " " << td->_default_channels[x];
			}
			sa << "\n";
		}
		for (AvailableChannels::RIter iter = td->_rtable.begin(); iter.live(); iter++) {
			AvailableChannels::DstInfo n = iter.value();
			sa << n._eth.unparse() << " ";
			for (int x = 0; x < n._channels.size(); x++) {
				sa << " " << n._channels[x];
			}
			sa << "\n";
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

int AvailableChannels::write_handler(const String &in_s, Element *e, void *vparam,
		ErrorHandler *errh) {
	AvailableChannels *f = (AvailableChannels *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	case H_INSERT:
		return f->parse_and_insert(in_s, errh);
	case H_REMOVE: {
		EtherAddress e;
		if (!cp_ethernet_address(s, &e))
			return errh->error("remove parameter must be ethernet address");
		f->_rtable.erase(e);
		break;
	}
	}
	return 0;
}

void AvailableChannels::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_read_handler("channels", read_handler, H_CHANNELS);
	add_write_handler("debug", write_handler, H_DEBUG);
	add_write_handler("insert", write_handler, H_INSERT);
	add_write_handler("remove", write_handler, H_REMOVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AvailableChannels)
