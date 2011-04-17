#ifndef CLICK_WINGQUERYRESPONDER_HH
#define CLICK_WINGQUERYRESPONDER_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGQueryResponder(IP, LT LinkTableMulti element, ARP ARPTableMulti element, [DEBUG])
 * =s Wifi, Wireless Routing
 * Responds to queries destined for this node.
 */

class WINGQueryResponder: public Element {
public:

	WINGQueryResponder();
	~WINGQueryResponder();

	const char *class_name() const { return "WINGQueryResponder"; }
	const char *port_count() const { return "2/1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &conf, ErrorHandler *errh);

	void push(int, Packet *);

protected:

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	bool _debug;

};

CLICK_ENDDECLS
#endif
