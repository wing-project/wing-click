/*
 * winghopcountmetric.{cc,hh} 
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
#include "winghopcountmetric.hh"
CLICK_DECLS

WINGHopCountMetric::WINGHopCountMetric() :
	WINGLinkMetric() {
}

WINGHopCountMetric::~WINGHopCountMetric() {
}

void *
WINGHopCountMetric::cast(const char *n) {
	if (strcmp(n, "WINGHopCountMetric") == 0)
		return (WINGHopCountMetric *) this;
	else if (strcmp(n, "WINGLinkMetric") == 0)
		return (WINGLinkMetric *) this;
	else
		return 0;
}

void WINGHopCountMetric::update_link(NodeAddress from, NodeAddress to,
		Vector<RateSize>, Vector<int>,
		Vector<int>, uint32_t seq, uint32_t channel) {

	if (!from || !to) {
		click_chatter("%{element} :: %s :: called with %s > %s", 
				this,
				__func__, 
				from.unparse().c_str(), 
				to.unparse().c_str());
		return;
	}

	/* update linktable */
	update_link_table(from, to, seq, 1, 1, channel);
}

EXPORT_ELEMENT(WINGHopCountMetric)
ELEMENT_REQUIRES(bitrate WINGLinkMetric)
CLICK_ENDDECLS
