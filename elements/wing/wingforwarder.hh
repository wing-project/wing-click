#ifndef CLICK_WINGFORWARDER_HH
#define CLICK_WINGFORWARDER_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGForwarder(IP, ARP ARPTableMulti element)
 * =s Wifi, Wireless Routing
 * Forwards source-routed packets.
 * =d
 * DSR-inspired ad-hoc routing protocol.
 * Input 0: packets that I receive off the wire
 * Output 0: Outgoing ethernet packets that I forward
 * Output 1: packets that were addressed to me.
 *
 */

class WINGForwarder: public Element {
public:

	WINGForwarder();
	~WINGForwarder();

	const char *class_name() const { return "WINGForwarder"; }
	const char *port_count() const { return "1/2"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);

	/* handler stuff */
	void add_handlers();
	int inc_packets() { return _inc_packets; }
	int inc_bytes() { return _inc_bytes; }
	int out_packets() { return _out_packets; }
	int out_bytes() { return _out_bytes; }
	String print_stats();

	void push(int, Packet *);

private:

	IPAddress _ip; // My address.

	class ARPTableMulti *_arp_table;

	/* statistics for handlers */
	int _inc_packets;
	int _inc_bytes;
	int _out_packets;
	int _out_bytes;

	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
