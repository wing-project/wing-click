/*
 * wingetxmetric.{cc,hh} 
 * John Bicket, Roberto Riggio, Stefano Testi
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
#include "wingetxmetric.hh"
CLICK_DECLS

WINGETXMetric::WINGETXMetric() :
	WINGLinkMetric() {
}

WINGETXMetric::~WINGETXMetric() {
}

void *
WINGETXMetric::cast(const char *n) {
	if (strcmp(n, "WINGETXMetric") == 0)
		return (WINGETXMetric *) this;
	else if (strcmp(n, "WINGLinkMetric") == 0)
		return (WINGLinkMetric *) this;
	else
		return 0;
}

void WINGETXMetric::update_link(NodeAddress from, NodeAddress to,
		Vector<RateSize> rs, Vector<int> fwd,
		Vector<int> rev, uint32_t seq, uint32_t channel) {

	if (!from || !to) {
		click_chatter("%{element} :: %s :: called with %s > %s", 
				this,
				__func__, 
				from.unparse().c_str(), 
				to.unparse().c_str());
		return;
	}

	int one_ack_fwd = 0;
	int one_ack_rev = 0;
	int six_ack_fwd = 0;
	int six_ack_rev = 0;

	/*
	 * if we don't have a few probes going out, just pick
	 * the smallest size for fwd rate
	 */
	int one_ack_size = 0;
	int six_ack_size = 0;

	for (int x = 0; x < rs.size(); x++) {
		if (rs[x]._rate == 2 && (!one_ack_size || one_ack_size > rs[x]._size)) {
			one_ack_size = rs[x]._size;
			one_ack_fwd = fwd[x];
			one_ack_rev = rev[x];
		} else if (rs[x]._rate == 12 && (!six_ack_size || six_ack_size
				> rs[x]._size)) {
			six_ack_size = rs[x]._size;
			six_ack_fwd = fwd[x];
			six_ack_rev = rev[x];
		}
	}

	if (!one_ack_fwd && !six_ack_fwd && !one_ack_rev && !six_ack_rev) {
		return;
	}

	int rev_metric = 0;
	int fwd_metric = 0;

	for (int x = 0; x < rs.size(); x++) {
		if (rs[x]._size >= 100) {
			int ack_rev = 0;
			if ((rs[x]._rate == 2) || (rs[x]._rate == 4) || (rs[x]._rate == 11) || (rs[x]._rate == 22)) {
				ack_rev = one_ack_rev;
			} else {
				ack_rev = six_ack_rev;
			}

			int metric = compute_metric(ack_rev, fwd[x]);

			if (!fwd_metric || (metric && metric < fwd_metric)) {
				fwd_metric = metric;
			}

			metric = compute_metric(ack_rev, fwd[x]);

			if (!rev_metric || (metric && metric < rev_metric)) {
				rev_metric = metric;
			}
		}
	}

	/* update linktable */
	update_link_table(from, to, seq, fwd_metric, rev_metric, channel);

}

EXPORT_ELEMENT(WINGETXMetric)
ELEMENT_REQUIRES(bitrate WINGLinkMetric)
CLICK_ENDDECLS
