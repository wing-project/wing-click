#ifndef CLICK_AGGREGATEWINGPACKETTYPE_HH
#define CLICK_AGGREGATEWINGPACKETTYPE_HH
#include <click/element.hh>
CLICK_DECLS

/*
 *=c
 * AggregateWingPacketType()
 * =s aggregates
 * sets aggregate annotation based on packet length
 * =d
 * AggregateWingPacketType sets the aggregate annotation on every 
 * passing packet to the packet's ethernet type.
 * =a
 * AggregateIP, AggregateCounter
 */

class AggregateWingPacketType : public Element { public:

    AggregateWingPacketType();
    ~AggregateWingPacketType();

    const char *class_name() const	{ return "AggregateWingPacketType"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return AGNOSTIC; }

    void push(int, Packet *);
    Packet *pull(int);

  private:

    Packet *handle_packet(Packet *);

};

CLICK_ENDDECLS
#endif
