#ifndef CLICK_WINGGATEWAYRESPONDER_HH
#define CLICK_WINGGATEWAYRESPONDER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include "nodeaddress.hh"
CLICK_DECLS

/*
 * =c
 * WINGGatewayResponder(IP, LT LinkTableMulti, SEL WINGGatewaySelector, RESPONDER WINGQueryResponder, [PERIOD], [DEBUG])
 * =s Wifi, Wireless Routing
 * Periodically generates route reply message addressed 
 * to this node current best gateway
 */

class WINGGatewayResponder: public Element {
public:

	WINGGatewayResponder();
	~WINGGatewayResponder();

	const char *class_name() const { return "WINGGatewayResponder"; }
	const char *port_count() const { return PORTS_0_1; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const { return "#/#"; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &conf, ErrorHandler *errh);
	void run_timer(Timer *);
	void add_handlers();

private:

	unsigned int _period; // msecs

	class WINGGatewaySelector *_gw_sel;
	class WINGQueryResponder *_responder;
	class LinkTableMulti *_link_table;

	Timer _timer;
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
