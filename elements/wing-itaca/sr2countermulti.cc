/*
 * counter.{cc,hh} -- element counts packets, measures packet rate
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Regents of the University of California
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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/handlercall.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/bitrate.hh>
#include "sr2countermulti.hh"
CLICK_DECLS

SR2CounterMulti::SR2CounterMulti()
  : _count(0), _byte_count(0)
{
}

SR2CounterMulti::~SR2CounterMulti()
{
}

uint32_t
SR2CounterMulti::read_count()
{
  return _count;
}

uint32_t
SR2CounterMulti::read_byte_count()
{
  return _byte_count;
}

uint32_t
SR2CounterMulti::read_busy_time()
{
  return _byte_count;
}

void
SR2CounterMulti::reset()
{
	_count = 0;
	_byte_count = 0;
	_busy_time = 0;
}

int
SR2CounterMulti::initialize(ErrorHandler *)
{
  reset();
  _discard = false;
  return 0;
}

void
SR2CounterMulti::push(int, Packet *p)
{
    
    if (!_discard){
        output(0).push(p);
    } else {
				struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
				int busy_time = calc_transmit_time(ceh->rate,p->length());
		    _count++;
		    _byte_count += p->length();
				_busy_time += busy_time;
        output(1).push(p);
    }
    
}


void
SR2CounterMulti::set_discard(bool discard)
{
    
    _discard = discard;
    
}

enum { H_COUNT, H_BYTE_COUNT, H_BUSY_TIME, H_RESET };

String
SR2CounterMulti::read_handler(Element *e, void *thunk)
{
    SR2CounterMulti *c = (SR2CounterMulti *)e;
    switch ((intptr_t)thunk) {
      case H_COUNT:
	return String(c->_count);
      case H_BYTE_COUNT:
	return String(c->_byte_count);
      case H_BUSY_TIME:
	return String(c->_busy_time);
      default:
	return "<error>";
    }
}

int
SR2CounterMulti::write_handler(const String &in_str, Element *e, void *thunk, ErrorHandler *errh)
{
    SR2CounterMulti *c = (SR2CounterMulti *)e;
    String str = in_str;
    switch ((intptr_t)thunk) {
      case H_RESET:
	c->reset();
	return 0;
      default:
	return errh->error("<internal>");
    }
}

void
SR2CounterMulti::add_handlers()
{
    add_read_handler("count", read_handler, (void *)H_COUNT);
    add_read_handler("byte_count", read_handler, (void *)H_BYTE_COUNT);
		add_read_handler("busy_time", read_handler, (void *)H_BUSY_TIME);
    add_write_handler("reset", write_handler, (void *)H_RESET, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SR2CounterMulti)
