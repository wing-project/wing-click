/*
 * radiotapencap.{cc,hh} -- encapsultates 802.11 packets
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
#include "radiotapencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include <clicknet/radiotap.h>
CLICK_DECLS

#define CLICK_RADIOTAP_PRESENT_FIRST (			\
	(1 << IEEE80211_RADIOTAP_RATE)			| \
	(1 << IEEE80211_RADIOTAP_DBM_TX_POWER)		| \
	(1 << IEEE80211_RADIOTAP_RTS_RETRIES)		| \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)		| \
	(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE)	| \
	(1 << IEEE80211_RADIOTAP_EXT)			| \
	0)

#define CLICK_RADIOTAP_PRESENT_MIDDLE (			\
	(1 << IEEE80211_RADIOTAP_RATE)			| \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)		| \
	(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE)	| \
	(1 << IEEE80211_RADIOTAP_EXT)			| \
	0)

#define CLICK_RADIOTAP_PRESENT_LAST (		\
	(1 << IEEE80211_RADIOTAP_RATE)		| \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)	| \
	(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE)	| \
	0)

RadiotapEncap::RadiotapEncap() {
}

RadiotapEncap::~RadiotapEncap() {
}

int
RadiotapEncap::configure(Vector<String> &conf, ErrorHandler *errh) {
	_debug = false;
	if (cp_va_kparse(conf, this, errh, "DEBUG", 0, cpBool, &_debug, cpEnd) < 0)
		return -1;
	return 0;
}

Packet *
RadiotapEncap::simple_action(Packet *p) {

	WritablePacket *p_out = p->uniqueify();
	click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	if (!p_out) {
		p->kill();
		return 0;
	}

	int size = sizeof(struct ieee80211_radiotap_header) + sizeof(u_int8_t) + sizeof(u_int8_t) + sizeof(u_int8_t) + sizeof(u_int8_t);
	if (ceh->rate1 > 0) {
		size += sizeof(u_int32_t) + sizeof(u_int8_t) + sizeof(u_int8_t);
		if (ceh->rate2 > 0) {
			size += sizeof(u_int32_t) + sizeof(u_int8_t) + sizeof(u_int8_t);
			if (ceh->rate3 > 0) {
				size += sizeof(u_int32_t) + sizeof(u_int8_t) + sizeof(u_int8_t);
			}
		}
	}

	p_out = p_out->push(size);

	if (p_out) {

		struct ieee80211_radiotap_header *crh  = (struct ieee80211_radiotap_header *) p_out->data();

		memset(crh, 0, size);

		crh->it_version = 0;
		crh->it_len = cpu_to_le16(size);
		crh->it_present = (ceh->rate1 == 0) ? cpu_to_le32(CLICK_RADIOTAP_PRESENT_LAST) : cpu_to_le32(CLICK_RADIOTAP_PRESENT_FIRST);

		uint32_t *ptr = (uint32_t *) (crh + 1);

		if (ceh->rate1 > 0) {
			*ptr = (ceh->rate2 == 0) ? cpu_to_le32(CLICK_RADIOTAP_PRESENT_LAST) : cpu_to_le32(CLICK_RADIOTAP_PRESENT_MIDDLE);
			ptr++;
			if (ceh->rate2 > 0) {
				*ptr = (ceh->rate3 == 0) ? cpu_to_le32(CLICK_RADIOTAP_PRESENT_LAST) : cpu_to_le32(CLICK_RADIOTAP_PRESENT_MIDDLE);
				ptr++;
				if (ceh->rate3 > 0) {
					*ptr = cpu_to_le32(CLICK_RADIOTAP_PRESENT_LAST);
					ptr++;
				}
			}
		}

		uint8_t *fld = (uint8_t *) (ptr);

		/* block (1) */
		*fld = ceh->rate;
		fld++;
		*fld = ceh->power;
		fld++;
		*fld = 0;
		fld++;
		if (ceh->retries > 0) {
			*fld = ceh->retries;
		} else if (ceh->max_tries > 0) {
			*fld = ceh->max_tries;
		} else {
			*fld = WIFI_MAX_RETRIES + 1;
		}
		fld++;

		/* block (1) */
		if (ceh->rate1) {
			*fld = ceh->rate1;
			fld++;
			if (ceh->max_tries1 > 0) {
				*fld = ceh->max_tries1;
			} else {
				*fld = WIFI_MAX_RETRIES + 1;
			}
			fld++;
			/* block (2) */
			if (ceh->rate2) {
				*fld = ceh->rate2;
				fld++;
				if (ceh->max_tries2 > 0) {
					*fld = ceh->max_tries1;
				} else {
					*fld = WIFI_MAX_RETRIES + 1;
				}
				fld++;
				/* block (3) */
				if (ceh->rate3) {
					*fld = ceh->rate3;
					fld++;
					if (ceh->max_tries3 > 0) {
						*fld = ceh->max_tries1;
					} else {
						*fld = WIFI_MAX_RETRIES + 1;
					}
					fld++;
				}
			}
		}
	}

	return p_out;
}

enum {H_DEBUG};

static String
RadiotapEncap_read_param(Element *e, void *thunk) {
	RadiotapEncap *td = (RadiotapEncap *)e;
	switch ((uintptr_t) thunk) {
		case H_DEBUG:
			return String(td->_debug) + "\n";
		default:
		return String();
	}
}

static int
RadiotapEncap_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	RadiotapEncap *f = (RadiotapEncap *)e;
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
RadiotapEncap::add_handlers()
{
	add_read_handler("debug", RadiotapEncap_read_param, (void *) H_DEBUG);
	add_write_handler("debug", RadiotapEncap_write_param, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapEncap)
