/*
 * radiotapdecap.{cc,hh} -- decapsultates 802.11 packets
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include "radiotapdecap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/radiotap_iter.h>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <clicknet/radiotap.h>
#include <clicknet/llc.h>
CLICK_DECLS

static void print_radiotap_namespace(struct ieee80211_radiotap_iterator *iter)
{
	switch (iter->this_arg_index) {
	case IEEE80211_RADIOTAP_TSFT:
		printf("\tTSFT: %llu\n", le64toh(*(unsigned long long *)iter->this_arg));
		break;
	case IEEE80211_RADIOTAP_FLAGS:
		printf("\tflags: %02x\n", *iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_RATE:
		printf("\trate: %lf\n", (double)*iter->this_arg/2);
		break;
	case IEEE80211_RADIOTAP_CHANNEL:
	case IEEE80211_RADIOTAP_FHSS:
	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
	case IEEE80211_RADIOTAP_DBM_ANTNOISE:
	case IEEE80211_RADIOTAP_LOCK_QUALITY:
	case IEEE80211_RADIOTAP_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DBM_TX_POWER:
	case IEEE80211_RADIOTAP_ANTENNA:
	case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
	case IEEE80211_RADIOTAP_DB_ANTNOISE:
	case IEEE80211_RADIOTAP_TX_FLAGS:
		break;
	case IEEE80211_RADIOTAP_RX_FLAGS:
		printf("\tRX flags: %#.4x\n",
			le16toh(*(uint16_t *)iter->this_arg));
		break;
	case IEEE80211_RADIOTAP_RTS_RETRIES:
		break;
	case IEEE80211_RADIOTAP_DATA_RETRIES:
		printf("\tretries: %d\n", *iter->this_arg);
		break;
	default:
		printf("\tBOGUS DATA\n");
		break;
	}
}

RadiotapDecap::RadiotapDecap()
{
}

RadiotapDecap::~RadiotapDecap()
{
}

int
RadiotapDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_debug = false;
	if (cp_va_kparse(conf, this, errh,
			"DEBUG", 0, cpBool, &_debug,
			cpEnd) < 0)
		return -1;
	return 0;
}

Packet *
RadiotapDecap::simple_action(Packet *p) {

	struct ieee80211_radiotap_header *th = (struct ieee80211_radiotap_header *) p->data();
	struct ieee80211_radiotap_iterator iter;
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	int err = ieee80211_radiotap_iterator_init(&iter, th, p->length(), 0);

	if (err) {
		click_chatter("%{element} :: %s :: malformed radiotap header (init returns %d)", this, __func__, err);
		goto drop;
	}

	memset((void*)ceh, 0, sizeof(struct click_wifi_extra));
	ceh->magic = WIFI_EXTRA_MAGIC;

	while (!(err = ieee80211_radiotap_iterator_next(&iter))) {
		print_radiotap_namespace(&iter);
		u_int16_t flags;
		switch (iter.this_arg_index) {
		case IEEE80211_RADIOTAP_FLAGS:
			flags = le16_to_cpu(*iter.this_arg);
			if (flags & IEEE80211_RADIOTAP_F_DATAPAD) {
				ceh->pad = 1;
			}
			if (flags & IEEE80211_RADIOTAP_F_FCS) {
				p->take(4);
			}
			break;
		case IEEE80211_RADIOTAP_RATE:
			ceh->rate = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DATA_RETRIES:
			ceh->retries = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_CHANNEL:
			ceh->channel = le16_to_cpu(*iter.this_arg);
			break;
		case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
			ceh->rssi = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DBM_ANTNOISE:
			ceh->silence = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
			ceh->rssi = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_DB_ANTNOISE:
			ceh->silence = *iter.this_arg;
			break;
		case IEEE80211_RADIOTAP_RX_FLAGS:
			flags = le16_to_cpu(*iter.this_arg);
			if (flags & IEEE80211_RADIOTAP_F_BADFCS)
				ceh->flags |= WIFI_EXTRA_RX_ERR;
			break;
		case IEEE80211_RADIOTAP_TX_FLAGS:
			flags = le16_to_cpu(*iter.this_arg);
			ceh->flags |= WIFI_EXTRA_TX;
			if (flags & IEEE80211_RADIOTAP_F_TX_FAIL)
				ceh->flags |= WIFI_EXTRA_TX_FAIL;
			break;

		default:
			click_chatter("%{element} :: %s :: unsupported field %d", 
					this, 
					__func__, 
					iter.this_arg_index);
			break;
		}
	}

	if (err != -ENOENT) {
		click_chatter("%{element} :: %s :: malformed radiotap data", this, __func__);
		goto drop;
	}

	p->pull(le16_to_cpu(th->it_len));
	p->set_mac_header(p->data());  // reset mac-header pointer

	return p;

  drop:

	p->kill();
	return 0;

}

enum {H_DEBUG};

static String
RadiotapDecap_read_param(Element *e, void *thunk)
{
	RadiotapDecap *td = (RadiotapDecap *)e;
	switch ((uintptr_t) thunk) {
		case H_DEBUG:
			return String(td->_debug) + "\n";
		default:
			return String();
	}
}

static int
RadiotapDecap_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
	RadiotapDecap *f = (RadiotapDecap *)e;
	String s = cp_uncomment(in_s);
	switch((intptr_t)vparam) {
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

void
RadiotapDecap::add_handlers()
{
	add_read_handler("debug", RadiotapDecap_read_param, (void *) H_DEBUG);
	add_write_handler("debug", RadiotapDecap_write_param, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapDecap)
