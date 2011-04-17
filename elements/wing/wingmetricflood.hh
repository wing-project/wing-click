#ifndef CLICK_WINGMETRICFLOOD_HH
#define CLICK_WINGMETRICFLOOD_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGMetricFlood(IP, LT LinkTable element, ARP ARPTable element, [DEBUG])
 * =s Wifi, Wireless Routing
 * =d
 * Floods route request messages
 */

// Query
class QueryInfo {
public:
	IPAddress _dst;
	IPAddress _src;
	QueryInfo() :
		_dst(IPAddress()),
		_src(IPAddress()) {
	}
	QueryInfo(IPAddress dst, IPAddress src) :
		_dst(dst),
		_src(src) {
	}
	QueryInfo(const QueryInfo &e) :
		_dst(e._dst) ,
		_src(e._src) { 
	}
	inline bool operator==(QueryInfo other) const {
		return (other._dst == _dst && other._src == _src);
	}
	String unparse() const {
		StringAccum sa;
		sa << _dst.unparse() << '/' << _src.unparse();
		return sa.take_string();
	}
};

class WINGMetricFlood: public WINGBase<QueryInfo> {
public:

	WINGMetricFlood();
	~WINGMetricFlood();

	const char *class_name() const { return "WINGMetricFlood"; }
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &conf, ErrorHandler *errh);

	/* handler stuff */
	void add_handlers();
	void push(int, Packet *);

	void start_query(IPAddress, int);
	void start_reply(PathMulti, uint32_t);

private:

	uint32_t _seq; // Next query sequence number to use.

	void start_flood(Packet *);
	void process_flood(Packet *);

	void forward_seen(int, Seen *);

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
