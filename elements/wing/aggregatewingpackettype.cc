// -*- mode: c++; c-basic-offset: 4 -*-

#include <click/config.h>
#include "aggregatewingpackettype.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>
#include "wingpacket.hh"
CLICK_DECLS

AggregateWingPacketType::AggregateWingPacketType()
{
}

AggregateWingPacketType::~AggregateWingPacketType()
{
}

Packet *
AggregateWingPacketType::handle_packet(Packet *p)
{
    click_ether *eh = (click_ether *) p->data();
    struct wing_header *pk = (struct wing_header *) (eh + 1);
    uint8_t type = pk->_type;
    SET_AGGREGATE_ANNO(p, type);
    return p;
}

void
AggregateWingPacketType::push(int, Packet *p)
{
    if (Packet *q = handle_packet(p))
	output(0).push(q);
}

Packet *
AggregateWingPacketType::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
	p = handle_packet(p);
    return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AggregateWingPacketType)
