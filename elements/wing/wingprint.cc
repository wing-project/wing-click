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
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include "wingprint.hh"
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
	return cp_va_kparse(conf, this, errh,
				"LABEL", cpkP, cpString, &_label,
				cpEnd);
}

String 
WINGPrint::wing_to_string(struct wing_packet *pk) 
{

	StringAccum sa;

	String type;
	switch (pk->_type) {
		case WING_PT_QUERY:
			type = "QUERY";
			break;
		case WING_PT_REPLY:
			type = "REPLY";
			break;
		case WING_PT_PROBE:
			type = "PROBE";
			break;
		case WING_PT_DATA:
			type = "DATA";
			break;
		case WING_PT_BCAST_DATA:
			type = "BROADCAST DATA";
			break;
		case WING_PT_GATEWAY:
			type = "GATEWAY";
			break;
		default:
			type = "UNKNOWN";
	}

	sa << type;

/*

  if (pk->_type == WING_PT_DATA) {
    sa << " len " << pk->hlen_with_data();
  } else {
    sa << " len " << pk->hlen_wo_data();
  }

  if (pk->_type == WING_PT_DATA) {
    sa << " dataseq " << pk->data_seq();
  }
  IPAddress qdst = pk->get_qdst();
  if (qdst) {
    sa << " qdst " << qdst;
  }

  if (pk->_type == WING_PT_DATA) {
    sa << " dlen=" << pk->data_len();
  }

  sa << " seq " << pk->seq();
  sa << " nhops " << pk->num_links();
  sa << " next " << pk->next();

  sa << " [";
  for(int i = 0; i< pk->num_links(); i++) {
    sa << " "<< pk->get_link_node(i).unparse() << " ";
    int fwd = pk->get_link_fwd(i);
    int rev = pk->get_link_rev(i);
    int seq = pk->get_link_seq(i);
    int age = pk->get_link_age(i);
    sa << "<" << fwd << " (" << seq << "," << age << ") " << rev << ">";
  }
  sa << " "<< pk->get_link_node(pk->num_links()).unparse() << " ";
  sa << "]";
*/
  return sa.take_string();

}

Packet *
WINGPrint::simple_action(Packet *p)
{
	StringAccum sa;
	if (_label[0] != 0) {
		sa << _label << ": ";
	} else {
		sa << "WINGPrint ";
	}
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh+1);
	sa << WINGPrint::wing_to_string(pk);
	click_chatter("%s", sa.c_str());
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WINGPrint)
