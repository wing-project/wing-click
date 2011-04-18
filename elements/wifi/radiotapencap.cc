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

#define CLICK_RADIOTAP_PRESENT (			\
	(1 << IEEE80211_RADIOTAP_RATE)			| \
	(1 << IEEE80211_RADIOTAP_DBM_TX_POWER)		| \
	(1 << IEEE80211_RADIOTAP_RTS_RETRIES)		| \
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)		| \
	0)

struct click_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int32_t	it_present1;
	u_int32_t	it_present2;
	u_int32_t	it_present3;
	u_int8_t	wt_rate;
	u_int8_t	wt_txpower;
	u_int8_t	wt_rts_retries;
	u_int8_t	wt_data_retries;
	u_int8_t	wt_rate1;
	u_int8_t	wt_data_retries1;
	u_int8_t	wt_rate2;
	u_int8_t	wt_data_retries2;
	u_int8_t	wt_rate3;
	u_int8_t	wt_data_retries3;
} __attribute__((__packed__));

RadiotapEncap::RadiotapEncap() {
}

RadiotapEncap::~RadiotapEncap() {
}

Packet *
RadiotapEncap::simple_action(Packet *p) {

	WritablePacket *p_out = p->uniqueify();
	click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

	if (!p_out) {
		p->kill();
		return 0;
	}

	p_out = p_out->push(sizeof(struct click_radiotap_header));

	if (!p_out) {
		p->kill();
		return 0;
	}

	struct click_radiotap_header *crh  = (struct click_radiotap_header *) p_out->data();

	memset(crh, 0, sizeof(struct click_radiotap_header));

	crh->wt_ihdr.it_version = 0;
	crh->wt_ihdr.it_len = cpu_to_le16(sizeof(struct click_radiotap_header));

	crh->wt_ihdr.it_present = cpu_to_le32(CLICK_RADIOTAP_PRESENT);

	crh->wt_rate = ceh->rate;
	crh->wt_txpower = ceh->power;
	crh->wt_rts_retries = 0;

	if (ceh->max_tries > 0) {
		crh->wt_data_retries = ceh->max_tries;
	} else {
		crh->wt_data_retries = WIFI_MAX_RETRIES + 1;
	}

	if (ceh->rate1 != 0) {
		crh->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
		crh->it_present |= cpu_to_le32(1 << IEEE80211_RADIOTAP_EXT);
		crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
		crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
		crh->wt_rate1 = ceh->rate1;
		if (ceh->max_tries1 > 0) {
			crh->wt_data_retries1 = ceh->max_tries1;
		} else {
			crh->wt_data_retries1 = WIFI_MAX_RETRIES + 1;
		}
		if (ceh->rate2 != 0) {
			crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
			crh->it_present1 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_EXT);
			crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
			crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
			crh->wt_rate2 = ceh->rate2;
			if (ceh->max_tries2 > 0) {
				crh->wt_data_retries2 = ceh->max_tries2;
			} else {
				crh->wt_data_retries2 = WIFI_MAX_RETRIES + 1;
			}
			if (ceh->rate3 != 0) {
				crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE);
				crh->it_present2 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_EXT);
				crh->it_present3 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_RATE);
				crh->it_present3 |= cpu_to_le32(1 << IEEE80211_RADIOTAP_DATA_RETRIES);
				crh->wt_rate3 = ceh->rate3;
				if (ceh->max_tries3 > 0) {
					crh->wt_data_retries3 = ceh->max_tries3;
				} else {
					crh->wt_data_retries3 = WIFI_MAX_RETRIES + 1;
				}
			}
		}
	}

	return p_out;

}

CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapEncap)
