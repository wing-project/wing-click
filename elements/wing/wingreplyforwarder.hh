#ifndef CLICK_WINGREPLYFORWARDER_HH
#define CLICK_WINGREPLYFORWARDER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include "pathmulti.hh"
CLICK_DECLS

/*
 * =c
 * WINGReplyForwarder(ETHERTYPE, IP, ETH, LinkTable element, ARPTable element)
 * =s Wifi, Wireless Routing
 * Responds to queries destined for this node.
 */

class WINGReplyForwarder: public Element {
public:

	WINGReplyForwarder();
	~WINGReplyForwarder();

	const char *class_name() const { return "WINGReplyForwarder"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const { return "#/#"; }

	int configure(Vector<String> &conf, ErrorHandler *errh);

	/* handler stuff */
	void add_handlers();

	void push(int, Packet *);

private:

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
