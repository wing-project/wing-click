/*
 * wingstripheader.{cc,hh}
 * John Bicket, Eddie Kohler, Roberto Riggio, Stefano Testi
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "wingstripheader.hh"
CLICK_DECLS

WINGStripHeader::WINGStripHeader() {
}

WINGStripHeader::~WINGStripHeader() {
}

Packet *
WINGStripHeader::simple_action(Packet *p) {
	click_ether *eh = (click_ether *) p->data();
	struct wing_data *pk = (struct wing_data *) (eh + 1);
	int extra = pk->hlen_wo_data() + sizeof(click_ether);
	p->pull(extra);
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WINGStripHeader)
