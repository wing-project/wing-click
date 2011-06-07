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
#include <click/args.hh>
CLICK_DECLS

WINGLinkMetric::WINGLinkMetric() :
	_link_table(0), _debug(false) {
}

WINGLinkMetric::~WINGLinkMetric() {
}

int WINGLinkMetric::configure(Vector<String> &conf, ErrorHandler *errh) {

	return Args(conf, this, errh)
		.read_m("LT", ElementCastArg("LinkTableMulti"), _link_table)
		.read("DEBUG", _debug)
		.complete();

}

void WINGLinkMetric::update_link_table(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t fwd, uint32_t rev, uint16_t channel) {

	if (_debug) {
		click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u) > %s",
				this, 
				__func__, 
				from.unparse().c_str(), 
				seq,
				fwd,
				channel,
				to.unparse().c_str());
	}
	if (fwd && _link_table && !_link_table->update_link(from, to, seq, 0, fwd, channel)) {
		click_chatter("%{element} :: %s :: couldn't update link %s > %d > %s",
				this, 
				__func__, 
				from.unparse().c_str(), 
				fwd,
				to.unparse().c_str());
	} 
	if (_debug) {
		click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u) > %s",
				this, 
				__func__, 
				to.unparse().c_str(), 
				seq,
				rev,
				channel,
				from.unparse().c_str());
	}
	if (rev && _link_table && !_link_table->update_link(to, from, seq, 0, rev, channel)) {
		click_chatter("%{element} :: %s :: couldn't update link %s > %d > %s",
				this, 
				__func__, 
				to.unparse().c_str(), 
				fwd,
				from.unparse().c_str());
	} 

}

void WINGLinkMetric::update_link(NodeAddress, NodeAddress, Vector<RateSize>, Vector<int>, Vector<int>, uint32_t, uint32_t) {
}

ELEMENT_REQUIRES(bitrate LinkTableMulti)
ELEMENT_PROVIDES(WINGLinkMetric)
