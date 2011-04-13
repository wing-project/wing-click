#ifndef CLICK_WINGQUERIER_HH
#define CLICK_WINGQUERIER_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGQuerier(IP, LT LinkTableMulti element, ARP ARPTableMulti element, [TIME_BEFORE_SWITCH], [QUERY_WAIT], [DEBUG])
 * =s Wifi, Wireless Routing
 * 
 * Set the Source Route for packet's destination inside the source route
 * header based on the destination ip annotation. If no source route is
 * found for a given packet and no valid route is found in the cache a
 * route request message is generated.
 *
 * =h add write
 * Writing "0:6.0.0.1;1 1:6.0.0.2:0" to this element will make all packets 
 * going from node 6.0.0.1 to node 6.0.0.2, use the first wireless interface
 * on both nodes.
 * 
 */

class WINGQuerier: public Element {
public:

	WINGQuerier();
	~WINGQuerier();

	const char *class_name() const { return "WINGQuerier"; }
	const char *port_count() const { return "1/2"; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const { return "#/#"; }

	int configure(Vector<String> &, ErrorHandler *);

	/* handler stuff */
	void add_handlers();
	String print_queries();
	String print_routes();

	void push(int, Packet *);
	Packet * encap(Packet *, PathMulti);
	void encap(Packet *);

private:

	class DstInfo {
	public:
		DstInfo() {}
		DstInfo(IPAddress ip) : _ip(ip) {}
		IPAddress _ip;
		int _best_metric;
		int _count;
		Timestamp _last_query;
		PathMulti _p;
		Timestamp _last_switch; // last time we picked a new best route
		Timestamp _first_selected; // when _p was first selected as best route
	};

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	bool _debug;

	typedef HashMap<IPAddress, DstInfo> DstTable;
	DstTable _queries;

	typedef HashMap<IPAddress, PathMulti> RouteTable;
	RouteTable _routes;

	Timestamp _query_wait;
	Timestamp _time_before_switch;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
