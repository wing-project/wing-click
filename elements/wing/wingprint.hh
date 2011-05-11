#ifndef CLICK_WINGPRINT_HH
#define CLICK_WINGPRINT_HH
#include <click/element.hh>
#include <click/string.hh>
#include "wingpacket.hh"
CLICK_DECLS

/*
 * =c
 * WINGPrint([TAG])
 * =d
 * Assumes input packets are WING packets. Prints out 
 * a description of those packets.
 */

class WINGPrint : public Element {

  public:

	WINGPrint();
	~WINGPrint();

	const char *class_name() const		{ return "WINGPrint"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);

	Packet *simple_action(Packet *);

  private:

	static String wing_to_string(struct wing_header *);
	String _label;

};

CLICK_ENDDECLS
#endif
