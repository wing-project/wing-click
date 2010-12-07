#ifndef CLICK_WINGQUERYRESPONDER_HH
#define CLICK_WINGQUERYRESPONDER_HH
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
 * WINGQueryResponder(ETHERTYPE, IP, ETH, LinkTable element, ARPTable element)
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
	const char *flow_code() const { return "#/#"; }

	int configure(Vector<String> &conf, ErrorHandler *errh);

	/* handler stuff */
	void add_handlers();

	void push(int, Packet *);
	void process_query(Packet *);
	void process_reply(Packet *);
	void start_reply(PathMulti, uint32_t);

private:

	IPAddress _ip; // My address.

	class Seen {
	public:
		Seen();
		Seen(IPAddress src, uint32_t seq) {
			_src = src;
			_seq = seq;
		}
		IPAddress _src;
		uint32_t _seq;
		PathMulti _last_response;
	};

	DEQueue<Seen> _seen;

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	int _max_seen_size;
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
