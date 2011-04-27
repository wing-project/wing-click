/*
 * settxrateht.{cc,hh} -- sets wifi txrate annotation on a packet
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/packet_anno.hh>
#include "settxrateht.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

SetTXRateHT::SetTXRateHT()
{
}

SetTXRateHT::~SetTXRateHT()
{
}

int
SetTXRateHT::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_mcs = 0;
	_et = 0;
	_offset = 0;
	_tries = WIFI_MAX_RETRIES + 1;
	if (cp_va_kparse(conf, this, errh,
			"MCS", cpkP, cpUnsigned, &_mcs,
			"TRIES", 0, cpUnsigned, &_tries,
			"ETHTYPE", 0, cpUnsignedShort, &_et,
			"OFFSET", 0, cpUnsigned, &_offset,
			cpEnd) < 0) {
		return -1;
	}
	if (_mcs < 0) {
		return errh->error("MCS must be >= 0");
	}
	if (_tries < 1) {
		return errh->error("TRIES must be >= 0");
	}
	return 0;
}

Packet *
SetTXRateHT::simple_action(Packet *p_in)
{
	uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
	click_ether *eh = (click_ether *) dst_ptr;

	if (_et && eh->ether_type != htons(_et)) {
		return p_in;
	}

	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
	ceh->magic = WIFI_EXTRA_MAGIC;
	ceh->mcs = _mcs;
	ceh->max_tries = _tries;

	return p_in;
}

enum {H_MCS, H_TRIES};

String
SetTXRateHT::read_handler(Element *e, void *thunk) 
{
	SetTXRateHT *f = (SetTXRateHT *) e;
	switch((uintptr_t) thunk) {
		case H_MCS: return String(f->_mcs) + "\n";
		case H_TRIES: return String(f->_tries) + "\n";
		default: return "\n";
	}
}

int
SetTXRateHT::write_handler(const String &arg, Element *e, void *vparam, ErrorHandler *errh)
{
	SetTXRateHT *f = (SetTXRateHT *) e;
	String s = cp_uncomment(arg);
	switch((intptr_t)vparam) {
		case H_MCS: {
			unsigned m;
			if (!cp_unsigned(s, &m))
				return errh->error("mcs parameter must be an unsigned");
			f->_mcs = m;
			break;
		}
		case H_TRIES: {
			unsigned m;
			if (!cp_unsigned(s, &m))
				return errh->error("tries parameter must be unsigned");
			f->_tries = m;
			break;
		}
	}
	return 0;
}

void
SetTXRateHT::add_handlers()
{
	add_read_handler("mcs", read_handler, (void *) H_MCS);
	add_read_handler("tries", read_handler, (void *) H_TRIES);
	add_write_handler("mcs", write_handler, (void *) H_MCS);
	add_write_handler("tries", write_handler, (void *) H_TRIES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXRateHT)

