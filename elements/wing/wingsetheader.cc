/*
 * wingsetheader.{cc,hh} 
 * John Bicket, Douglas S. J. De Couto, Robert Morris, Roberto Riggio, Stefano Testi
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
#include "wingsetheader.hh"
CLICK_DECLS

WINGSetHeader::WINGSetHeader() {
}

WINGSetHeader::~WINGSetHeader() {
}

Packet *
WINGSetHeader::simple_action(Packet *p_in) {
	WritablePacket *p = p_in->uniqueify();
	if (!p) {
		return 0;
	}
	click_ether *eh = (click_ether *) p->data();
	eh->ether_type = htons(_wing_et);
	struct wing_header *pk = (struct wing_header *) (eh + 1);
	if (!pk) {
		click_chatter("%{element} :: %s :: bad packet", this, __func__);
		goto bad;
	}
	pk->_version = _wing_version;
	switch (pk->_type) {
	case WING_PT_QUERY : 
	case WING_PT_REPLY : 
	case WING_PT_GATEWAY : 
		if (p->length() < sizeof(struct wing_packet)) {
			click_chatter("%{element} :: %s :: packet truncated", this, __func__);
			goto bad;
		}
		((struct wing_packet *) (eh + 1))->set_checksum();
		break;
	case WING_PT_DATA : 
		if (p->length() < sizeof(struct wing_data)) {
			click_chatter("%{element} :: %s :: packet truncated", this, __func__);
			goto bad;
		}
		((struct wing_data *) (eh + 1))->set_checksum();
		break;
	case WING_PT_BCAST_DATA : 
		if (p->length() < sizeof(struct wing_bcast_data)) {
			click_chatter("%{element} :: %s :: packet truncated", this, __func__);
			goto bad;
		}
		((struct wing_bcast_data *) (eh + 1))->set_checksum();
		break;
	case WING_PT_PROBE : 
		if (p->length() < sizeof(struct wing_probe)) {
			click_chatter("%{element} :: %s :: probe truncated", this, __func__);
			goto bad;
		}
		((struct wing_probe *) (eh + 1))->set_checksum();
		break;
	default : 
		click_chatter("%{element} :: %s :: bad packet type %d", this, __func__, pk->_type);
		goto bad;
	}
	return p;
	bad: 
	p->kill();
	return (0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WINGSetHeader)
