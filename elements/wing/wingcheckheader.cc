/*
 * wingcheckheader.{cc,hh}
 * John Bicket, Douglas S. J. De Couto, Robert Morris, Stefano Testi, Roberto Riggio
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "wingcheckheader.hh"
CLICK_DECLS

WINGCheckHeader::WINGCheckHeader() :
	_drops(0) {
}

WINGCheckHeader::~WINGCheckHeader() {
}

Packet *
WINGCheckHeader::simple_action(Packet *p) {

	click_ether *eh = (click_ether *) p->data();
	struct wing_header *pk = (struct wing_header *) (eh + 1);
	bool cksum = false;

	if (!pk) {
		goto bad;
	}

	// check packet length
	int len;
	switch (pk->_type) {
	case WING_PT_QUERY : 
	case WING_PT_REPLY : 
	case WING_PT_GATEWAY : 
		len = ((struct wing_packet *) (eh + 1))->hlen_wo_data();
		break;
	case WING_PT_PROBE : 
		len = ((struct wing_probe *) (eh + 1))->size();
		break;
	case WING_PT_DATA : 
		len = ((struct wing_data *) (eh + 1))->hlen_w_data();
		break;
	case WING_PT_BCAST_DATA : 
		len = ((struct wing_bcast_data *) (eh + 1))->hlen_w_data();
		break;
	default : 
		click_chatter("%{element} :: %s :: bad packet type %d", this, __func__, ntohs(pk->_type));
		goto bad;
	}

	if (len + sizeof(click_ether) != p->length()) {
		click_chatter("%{element} :: %s :: wrong packet length, claimed %d, real %d", 
				this, 
				__func__,
				len + sizeof(click_ether),
				p->length());
		goto bad;
	}

	// check checksum
	switch (pk->_type) {
	case WING_PT_QUERY : 
	case WING_PT_REPLY : 
	case WING_PT_GATEWAY : 
		cksum = ((struct wing_packet *) (eh + 1))->check_checksum();
		break;
	case WING_PT_PROBE : 
		cksum = ((struct wing_probe *) (eh + 1))->check_checksum();
		break;
	case WING_PT_DATA : 
		cksum = ((struct wing_data *) (eh + 1))->check_checksum();
		break;
	case WING_PT_BCAST_DATA : 
		cksum = ((struct wing_bcast_data *) (eh + 1))->check_checksum();
		break;
	}
	if (!cksum) {
		click_chatter("%{element} :: %s :: failed checksum from %s, packet type %02x", 
				this, 
				__func__,
				EtherAddress(eh->ether_shost).unparse().c_str(),
				pk->_type);
		goto bad;
	}

	// check packet version
	if (pk->_version != _wing_version) {
		_bad_table.insert(EtherAddress(eh->ether_shost), pk->_version);
		click_chatter("%{element} :: %s :: unknown protocol version %02x from %s",
				this, 
				__func__, 
				pk->_version,
				EtherAddress(eh->ether_shost).unparse().c_str());

		goto bad;
	}

	return (p);
	bad: 
	_drops++;
	if (noutputs() == 2) {
		output(1).push(p);
	} else {
		p->kill();
	}
	return 0;
}

String WINGCheckHeader::bad_nodes() {
	StringAccum sa;
	for (BadTable::const_iterator i = _bad_table.begin(); i.live(); i++) {
		uint8_t version = i.value();
		EtherAddress dst = i.key();
		sa << this << " eth " << dst.unparse() << " version " << (int) version << "\n";
	}
	return sa.take_string();
}

enum {
	H_DROPS, H_BAD_NODES
};

String WINGCheckHeader::read_handler(Element *e, void *thunk) {
	WINGCheckHeader *td = (WINGCheckHeader *) e;
	switch ((uintptr_t) thunk) {
	case H_DROPS:
		return String(td->drops()) + "\n";
	case H_BAD_NODES:
		return td->bad_nodes();
	default:
		return String() + "\n";
	}
}

void WINGCheckHeader::add_handlers() {
	add_read_handler("drops", read_handler, H_DROPS);
	add_read_handler("bad_nodes", read_handler, H_BAD_NODES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WINGCheckHeader)
