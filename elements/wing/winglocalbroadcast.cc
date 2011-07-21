/*
 * winglocalbroadcast.{cc,hh}
 * John Bicket, Roberto Riggio, Stefano Testi
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include "winglocalbroadcast.hh"
#include <click/args.hh>
CLICK_DECLS

WINGLocalBroadcast::WINGLocalBroadcast() {
}

WINGLocalBroadcast::~WINGLocalBroadcast() {
}

int WINGLocalBroadcast::configure(Vector<String> &conf, ErrorHandler *errh) {

	return Args(conf, this, errh)
		.read_m("IP", _ip)
		.read_m("LT", ElementCastArg("LinkTableMulti"), _link_table)
		.read_m("ARP", ElementCastArg("ARPTableMulti"), _arp_table)
		.complete();

}

void 
WINGLocalBroadcast::push(int, Packet *p_in) 
{

	int data_len = p_in->length();

	WritablePacket *p = p_in->push(sizeof(wing_bcast_data) + sizeof(click_ether));

	if (p == 0) {
		click_chatter("%{element} :: %s :: cannot encap packet!", this, __func__);
		return;
	}

	click_ether *eh = (click_ether *) p->data();
	struct wing_bcast_data *pk = (struct wing_bcast_data *) (eh + 1);
	memset(pk, '\0', sizeof(wing_bcast_data));

	pk->_type = WING_PT_BCAST_DATA;
	pk->set_data_len(data_len);

	Vector<int> ifs = _link_table->get_local_interfaces();

	NodeAddress src = NodeAddress(_ip, ifs[0]);
	EtherAddress eth_src = _arp_table->lookup(src);

	if (src && eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				src.unparse().c_str(),
				eth_src.unparse().c_str());
	}

	memset(eh->ether_dhost, 0xff, 6);
	memcpy(eh->ether_shost, eth_src.data(), 6);

	for (int i = 1; i < ifs.size(); i++) {
		if (WritablePacket *q = p->clone()->uniqueify()) {
			eh = (click_ether *) q->data();
			src = NodeAddress(_ip, ifs[i]);
			eth_src = _arp_table->lookup(src);
			if (src && eth_src.is_group()) {
				click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
						this,
						__func__,
						src.unparse().c_str(),
						eth_src.unparse().c_str());
			}
			memcpy(eh->ether_shost, eth_src.data(), 6);
			output(0).push(q);
		}
	}

	output(0).push(p);

}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ARPTableMulti LinkTableMulti)
EXPORT_ELEMENT(WINGLocalBroadcast)
