#ifndef CLICK_WINGLOCALBROADCAST_HH
#define CLICK_WINGLOCALBROADCAST_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGLocalBroadcast(IP, LT LinkTableMulti element, ARP ARPTableMulti element)
 * =s Wifi, Wireless Routing
 *
 */

class WINGLocalBroadcast: public Element {
public:

	WINGLocalBroadcast();
	~WINGLocalBroadcast();

	const char *class_name() const { return "WINGLocalBroadcast"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const { return "#/#"; }

	int configure(Vector<String> &, ErrorHandler *);

	void push(int, Packet *);

private:

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

};

CLICK_ENDDECLS
#endif
