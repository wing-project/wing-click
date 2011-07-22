#ifndef CLICK_WINGGATEWAYRESPONDER_HH
#define CLICK_WINGGATEWAYRESPONDER_HH
#include <click/element.hh>
#include "wingbase.hh"
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
	const char *port_count() const { return PORTS_0_0; }
	const char *processing() const { return AGNOSTIC; }
	
	int initialize(ErrorHandler *);
	int configure(Vector<String> &conf, ErrorHandler *errh);
	void run_timer(Timer *);

private:

	unsigned int _period; // msecs

	class WINGGatewaySelector *_gw_sel;
	class WINGMetricFlood *_metric_flood;
	class LinkTableMulti *_link_table;

	Timer _timer;

};

CLICK_ENDDECLS
#endif
