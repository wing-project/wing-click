#ifndef CLICK_WINGLOCALBROADCAST_HH
#define CLICK_WINGLOCALBROADCAST_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGLocalBroadCast(IP, LT LinkTableMulti element, ARP ARPTableMulti element, [TIME_BEFORE_SWITCH], [QUERY_WAIT], [DEBUG])
 * =s Wifi, Wireless Routing
 *
 */

class WINGLocalBroadCast: public Element {
public:

	WINGLocalBroadCast();
	~WINGLocalBroadCast();

	const char *class_name() const { return "WINGLocalBroadCast"; }
	const char *port_count() const { return "1/2"; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const { return "#/#"; }

	int configure(Vector<String> &, ErrorHandler *);

	void push(int, Packet *);

private:

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	bool _debug;

};

CLICK_ENDDECLS
#endif
