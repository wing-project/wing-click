#ifndef CLICK_WINGSTRIPHEADER_HH
#define CLICK_WINGSTRIPHEADER_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGStripHeader()
 * =s Wifi, Wireless Routing
 * Strips outermost Wing header
 * =d
 * Removes the outermost Wing header.
 * =a WINGCheckHeader
 */

class WINGStripHeader: public Element {

public:

	WINGStripHeader();
	~WINGStripHeader();

	const char *class_name() const { return "WINGStripHeader"; }
	const char *port_count() const { return PORTS_1_1; }

	Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
