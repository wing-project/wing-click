#ifndef CLICK_WINGSETHEADER_HH
#define CLICK_WINGSETHEADER_HH

/*
 * =c
 * WINGSetHeader()
 * =s Wifi, Wireless Routing
 * Set header for Source Routed packet.
 * =d
 * Expects a Wing packet as input. Calculates the header's checksum 
 * and sets the version and checksum header fields.
 * =a WINGCheckHeader 
 */

#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

class WINGSetHeader: public Element {
public:
	WINGSetHeader();
	~WINGSetHeader();

	const char *class_name() const { return "WINGSetHeader"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
