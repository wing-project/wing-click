/*
 * radiotapencapht.{cc,hh} -- encapsultates HT 802.11 packets
 * John Bicket, Roberto Riggio
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
#include "radiotapencapht.hh"
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
	(1 << IEEE80211_RADIOTAP_DATA_RETRIES)		| \
	(1 << IEEE80211_RADIOTAP_MCS)			| \
	0)

struct click_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int8_t	wt_data_retries;
	u_int8_t	wt_known;
	u_int8_t	wt_flags;
	u_int8_t	wt_mcs;
} __attribute__((__packed__));

RadiotapEncapHT::RadiotapEncapHT() {
}

RadiotapEncapHT::~RadiotapEncapHT() {
}

Packet *
RadiotapEncapHT::simple_action(Packet *p) {

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

	crh->wt_known |= IEEE80211_RADIOTAP_MCS_HAVE_BW | 
	                 IEEE80211_RADIOTAP_MCS_HAVE_MCS | 
	                 IEEE80211_RADIOTAP_MCS_HAVE_GI;

	crh->wt_flags |= IEEE80211_RADIOTAP_MCS_BW_40 | 
	                 IEEE80211_RADIOTAP_MCS_SGI;

	crh->wt_mcs = ceh->mcs;

	if (ceh->max_tries > 0) {
		crh->wt_data_retries = ceh->max_tries;
	} else {
		crh->wt_data_retries = WIFI_MAX_RETRIES + 1;
	}

	return p_out;

}

CLICK_ENDDECLS
EXPORT_ELEMENT(RadiotapEncapHT)
