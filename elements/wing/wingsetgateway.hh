#ifndef CLICK_SETGATEWAY_HH
#define CLICK_SETGATEWAY_HH
#include <click/element.hh>
#include "wingbase.hh"
#include <click/ipflowid.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

/*
 * =c
 * WINGSetGateway([PERIOD], [DEBUG])
 * =s Wifi, Wireless Routing
 * WINGSetGateway a TCP flow current default route.
 * =d
 * This element marks the gateway for a packet to be sent to.
 */

class WINGSetGateway: public Element {
public:

	WINGSetGateway();
	~WINGSetGateway();

	const char *class_name() const { return "WINGSetGateway"; }
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }
	const char *flow_code() const { return "#/#"; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &conf, ErrorHandler *errh);

	/* handler stuff */
	void add_handlers();
	String print_flows();

	void push(int, Packet *);
	void run_timer(Timer *);

private:

	class FlowTableEntry {
	public:
		class IPFlowID _id;
		IPAddress _gw;
		Timestamp _oldest_unanswered;
		Timestamp _last_reply;
		int _outstanding_syns;
		bool _fwd_alive;
		bool _rev_alive;
		bool _all_answered;
		FlowTableEntry() {
			_all_answered = true;
			_fwd_alive = true;
			_rev_alive = true;
		}
		FlowTableEntry(const FlowTableEntry &e) :
			_id(e._id), _gw(e._gw), _oldest_unanswered(e._oldest_unanswered),
			_last_reply(e._last_reply), _outstanding_syns(e._outstanding_syns), 
			_fwd_alive(e._fwd_alive), _rev_alive(e._rev_alive), 
			_all_answered(e._all_answered) {
		}
		void saw_forward_packet() {
			if (_all_answered) {
				_oldest_unanswered = Timestamp::now();
				_all_answered = false;
			}
		}
		void saw_reply_packet() {
			_last_reply = Timestamp::now();
			_all_answered = true;
			_outstanding_syns = 0;
		}
		bool is_pending() const {
			return (_outstanding_syns > 0);
		}
		Timestamp age() {
			return Timestamp::now() - _last_reply;
		}
	};

	typedef HashMap<IPFlowID, FlowTableEntry> FlowTable;
	typedef FlowTable::const_iterator FTIter;
	FlowTable _flow_table;

	IPAddress _gw;
	Timer _timer;
	uint32_t _period;
	bool _debug;

	class WINGGatewaySelector *_gw_sel;

	void push_fwd(Packet *);
	void push_rev(Packet *);

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
