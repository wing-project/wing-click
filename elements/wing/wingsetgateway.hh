#ifndef CLICK_WINGSETGATEWAY_HH
#define CLICK_WINGSETGATEWAY_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include "pathmulti.hh"
#include <click/ipflowid.hh>
#include <clicknet/tcp.h>
#include "arptablemulti.hh"
#include "winggatewayselector.hh"
CLICK_DECLS

/*
 * =c
 * WINGSetGateway([GW ipaddress], [SEL WINGGatewaySelector element], [DEBUG])
 * =d
 * This element marks the gateway for a packet to be sent to.
 * Either manually specifiy an gw using the GW keyword
 * or automatically select it using a GatewaySelector element.
 */

class WINGSetGateway: public Element {
public:

	WINGSetGateway();
	~WINGSetGateway();

	const char *class_name() const { return "WINGSetGateway"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const { return "#/#"; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &conf, ErrorHandler *errh);

	/* handler stuff */
	void add_handlers();

	void push(int, Packet *);

private:

	class WINGGatewaySelector *_gw_sel;
	IPAddress _gw;
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
