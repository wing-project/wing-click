/*
 * wingprint.{cc,hh} -- print wing packets
 * Roberto Riggio
 *
 * Copyright (c) 2011 CREATE-NET
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
#include "wingprint.hh"
#include <click/args.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
CLICK_DECLS

WINGPrint::WINGPrint()
{
}

WINGPrint::~WINGPrint()
{
}

int
WINGPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
	return Args(conf, this, errh).read_p("LABEL", _label).complete();
}

String 
WINGPrint::wing_to_string(struct wing_header *hdr) 
{
	StringAccum sa;
	switch (hdr->_type) {
		case WING_PT_PROBE: {
			struct wing_probe *pk = (struct wing_probe *) hdr;
			sa << " rate " << (int) pk->rate();
			sa << " channel " << (int) pk->channel();
			sa << " size " << (int) pk->size();
			sa << " neighbor " << pk->node();
			sa << " seq " << (int) pk->seq();
			sa << " period " << (int) pk->period();
			sa << " tau " << (int) pk->tau();
			sa << " sent " << (int) pk->sent();
			break;
		}
		case WING_PT_REPLY:
		case WING_PT_QUERY: {
			struct wing_packet *pk = (struct wing_packet *) hdr;
			int seq = pk->seq();
			int num_links = pk->num_links();
			int next = pk->next();
			sa << " len " << pk->hlen_wo_data();
			sa << " dst " << pk->qdst();
			sa << " src " << pk->qsrc();
			sa << " seq " << seq;
			sa << " nlinks " << num_links;
			sa << " next " << next;
			sa << " [";
			for(int i = 0; i< pk->num_links(); i++) {
				sa << route_to_string(pk->get_path());
			}
			sa << "]";
			break;
		}
		case WING_PT_DATA: {
			struct wing_data *pk = (struct wing_data *) hdr;
			int num_links = pk->num_links();
			int next = pk->next();
			sa << " len " << pk->hlen_w_data();
			sa << " nlinks " << num_links;
			sa << " next " << next;
			sa << " [";
			for(int i = 0; i< pk->num_links(); i++) {
				sa << route_to_string(pk->get_path());
			}
			sa << "]";
			break;
			break;
		}
		case WING_PT_BCAST_DATA: {
			struct wing_bcast_data *pk = (struct wing_bcast_data *) hdr;
			sa << " len " << pk->hlen_w_data();
			break;
		}
		case WING_PT_GATEWAY: {
			struct wing_packet *pk = (struct wing_packet *) hdr;
			int seq = pk->seq();
			int num_links = pk->num_links();
			sa << " len " << pk->hlen_wo_data();
			sa << " route " << pk->qdst() << "/" << pk->netmask() << " " << pk->qsrc();
			sa << " seq " << seq;
			sa << " nlinks " << num_links;
			sa << " [";
			for(int i = 0; i< pk->num_links(); i++) {
				sa << route_to_string(pk->get_path());
			}
			sa << "]";
			break;
		}
	}
	return sa.take_string();
}

Packet *
WINGPrint::simple_action(Packet *p)
{
	StringAccum sa;
	if (_label[0] != 0) {
		sa << _label << ": ";
	}
	click_ether *eh = (click_ether *) p->data();
	int len;
	len = sprintf(sa.reserve(9), "%4d | ", p->length());
	sa.adjust_length(len);
	struct wing_header *pk = (struct wing_header *) (eh+1);
	switch (pk->_type) {
		case WING_PT_QUERY:
			sa << "query";
			break;
		case WING_PT_REPLY:
			sa << "reply";
			break;
		case WING_PT_PROBE:
			sa << "probe";
			break;
		case WING_PT_DATA:
			sa << "data";
			break;
		case WING_PT_BCAST_DATA:
			sa << "broadcast";
			break;
		case WING_PT_GATEWAY:
			sa << "hna";
			break;
		default:
			sa << "unknown";
	}
	sa << " " << EtherAddress(eh->ether_dhost);
	sa << " " << EtherAddress(eh->ether_shost);
	sa << WINGPrint::wing_to_string(pk);
	click_chatter("%s", sa.c_str());
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WINGPrint)
