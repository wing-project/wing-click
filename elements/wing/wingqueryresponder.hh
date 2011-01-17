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
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGQueryResponder(ETHERTYPE, IP, ETH, LinkTable element, ARPTable element)
 * =s Wifi, Wireless Routing
 * Responds to queries destined for this node.
 */

// Query
class ReplyInfo {
public:
	IPAddress _src;
	PathMulti _last_response;
	ReplyInfo() :
		_src(IPAddress()),
		_last_response(PathMulti()) {
	}
	ReplyInfo(IPAddress src, PathMulti last_response) :
		_src(src),
		_last_response(last_response) {
	}
	ReplyInfo(const ReplyInfo &e) :
		_src(e._src) ,
		_last_response(e._last_response) { 
	}
	inline bool operator==(ReplyInfo other) const {
		return (other._src == _src && other._last_response == _last_response);
	}
	String unparse() const {
		StringAccum sa;
		sa << _src.unparse();
		if (_last_response.size() == 0) {
			sa << "empty route";
		} else {
			sa << " (" << route_to_string(_last_response) << ")";
		}
		return sa.take_string();
	}
};

class WINGQueryResponder: public WINGBase<ReplyInfo> {
public:

	WINGQueryResponder();
	~WINGQueryResponder();

	const char *class_name() const { return "WINGQueryResponder"; }
	const char *port_count() const { return "2/1"; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &conf, ErrorHandler *errh);

	void push(int, Packet *);
	void process_query(Packet *);
	void process_reply(Packet *);
	void start_reply(PathMulti, uint32_t);

protected:

	void forward_seen(int, Seen *) {}
};

CLICK_ENDDECLS
#endif
