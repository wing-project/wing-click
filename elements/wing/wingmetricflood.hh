#ifndef CLICK_WINGMETRICFLOOD_HH
#define CLICK_WINGMETRICFLOOD_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include "wingbase.hh"
#include "nodeaddress.hh"
CLICK_DECLS

/*
 * =c
 * WINGMetricFlood(ETHERTYPE, IP, ETH, LinkTable element, ARPTable element)
 * =s Wifi, Wireless Routing
 * =d
 * Floods a packet with previous hops based on Link Metrics.
 */

class WINGMetricFlood: public WINGBase {
public:

	WINGMetricFlood();
	~WINGMetricFlood();

	const char *class_name() const { return "WINGMetricFlood"; }
	void *cast(const char *);
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const {	return "#/#"; }

	int configure(Vector<String> &conf, ErrorHandler *errh);

	/* handler stuff */
	void add_handlers();

	void push(int, Packet *);

	static void static_forward_query_hook(Timer *t, void *e) {
		delete t;
		((WINGMetricFlood *) e)->forward_query_hook();
	}

private:

	// List of query sequence #s that we've already seen.
	class Seen {
	public:
		Seen();
		Seen(IPAddress src, IPAddress dst, uint32_t seq) {
			_src = src;
			_dst = dst;
			_seq = seq;
			_count = 0;
			_when = Timestamp::now();
		}
		IPAddress _src;
		IPAddress _dst;
		uint32_t _seq;
		int _count;
		Timestamp _when;
		Timestamp _to_send;
		bool _forwarded;
	};

	DEQueue<Seen> _seen;

	uint32_t _seq; // Next query sequence number to use.
	uint32_t _jitter; // msecs
	int _max_seen_size;

	void start_flood(Packet *);
	void process_flood(Packet *);

	void start_query(IPAddress, int);
	void forward_query(int, Seen *);
	void forward_query_hook();

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
