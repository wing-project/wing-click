#ifndef CLICK_WINGROUTERESPONDER_HH
#define CLICK_WINGROUTERESPONDER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
CLICK_DECLS

/*
 * =c
 * WINGRouteResponder(IP, ARP ARPTableMulti element, LT LinkTableMulti, [DEBUG])
 * =s Wifi, Wireless Routing
 * Generates route error messages.
 *
 */

class WINGRouteResponder: public Element {
public:

	WINGRouteResponder();
	~WINGRouteResponder();

	const char *class_name() const { return "WINGRouteResponder"; }
	const char *port_count() const { return "1/0-1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);

	void push(int, Packet *);

	/* handler stuff */
	void add_handlers();

private:

	IPAddress _ip; // My address.
	bool _debug;

	class ARPTableMulti *_arp_table;
	class LinkTableMulti *_link_table;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
