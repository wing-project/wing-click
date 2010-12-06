/*
 * winglinkmetric.{cc,hh} 
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
#include "winglinkmetric.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
CLICK_DECLS

WINGLinkMetric::WINGLinkMetric() :
	_link_table(0), _debug(false) {
}

WINGLinkMetric::~WINGLinkMetric() {
}

int WINGLinkMetric::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (cp_va_kparse(conf, this, errh, 
				"LT", cpkM, cpElement, &_link_table,
				"DEBUG", 0, cpBool, &_debug, 
			cpEnd) < 0)
		return -1;

	if (_link_table && _link_table->cast("LinkTableMulti") == 0)
		return errh->error("LinkTable element is not a LinkTableMulti");

	return 0;
}

void WINGLinkMetric::update_link(NodeAddress, NodeAddress, Vector<RateSize>, Vector<int>, Vector<int>, uint32_t, uint32_t) {
}

ELEMENT_REQUIRES(bitrate LinkTableMulti)
ELEMENT_PROVIDES(WINGLinkMetric)
