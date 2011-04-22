#ifndef CLICK_WINGGATEWAYSELECTOR_HH
#define CLICK_WINGGATEWAYSELECTOR_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGGatewaySelector(IP, LT LinkTableMulti element, ARP ARPTableMulti element, [PERIOD], [EXPIRE], [DEBUG])
 * =s Wifi, Wireless Routing
 * Select a gateway to send a packet to based on TCP 
 * connection state and metric to gateway.
 * =d
 * This element provides proactive gateway selection.  
 * Each gateway broadcasts an ad every PERIOD msec.  
 * Non-gateway nodes select the gateway with the best 
 * metric and forward ads.
 */

// Host-Network Associations
class HNAInfo {
public:
	IPAddress _dst;
	IPAddress _nm;
	IPAddress _gw;
	HNAInfo() :
		_dst(IPAddress()),
		_nm(IPAddress()),
		_gw(IPAddress()) {
	}
	HNAInfo(IPAddress dst, IPAddress nm, IPAddress gw) :
		_dst(dst),
		_nm(nm),
		_gw(gw) {
	}
	HNAInfo(const HNAInfo &e) :
		_dst(e._dst) ,
		_nm(e._nm), 
		_gw(e._gw) { 
	}
	inline bool contains(IPAddress a) const {
		return a.matches_prefix(_dst, _nm);
	}
	inline uint32_t hashcode() const {
		return CLICK_NAME(hashcode)(_dst) + CLICK_NAME(hashcode)(_nm) + CLICK_NAME(hashcode)(_gw);
	}
	inline bool operator==(HNAInfo other) const {
		return (other._dst == _dst && other._nm == _nm && other._gw == _gw);
	}
	String unparse() const {
		StringAccum sa;
		sa << _dst.unparse() << ' ' << _nm.unparse() << ' ' << _gw.unparse();
		return sa.take_string();
	}
};

class WINGGatewaySelector : public WINGBase<HNAInfo> {
public:

	WINGGatewaySelector();
	~WINGGatewaySelector();

	const char *class_name() const { return "WINGGatewaySelector"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &conf, ErrorHandler *errh);

	/* handler stuff */
	void add_handlers();
	String print_gateway_stats();

	void push(int, Packet *);
	void run_timer(Timer *);

	void hnas_clear();
	int hna_add(IPAddress, IPAddress);
	int hna_del(IPAddress, IPAddress);
	String hnas();

	IPAddress best_gateway(IPAddress);
	IPAddress best_gateway() {
		return best_gateway(IPAddress());
	}

	bool is_gateway(IPAddress dst, IPAddress nm) {
		for (int x = 0; x < _hnas.size(); x++) {
			if (_hnas[x]._dst == dst && _hnas[x]._nm == nm) {
				return true;
			}
		}
		return false;
	}
	bool is_gateway() {
		return is_gateway(IPAddress(), IPAddress());
	}

private:

	// List of gateways we already known
	class GWInfo {
	public:
		GWInfo() : 
			_hna(HNAInfo()), 
			_first_update(Timestamp::now()),
			_seen(0) {
		}
		GWInfo(HNAInfo hna) : 
			_hna(hna), 
			_first_update(Timestamp::now()),
			_seen(0) {
		}
		GWInfo(const GWInfo &e) :
			_hna(e._hna), 
			_first_update(e._first_update), 
			_last_update(e._last_update), 
			_seen(e._seen) {
		}
		HNAInfo _hna;
		Timestamp _first_update;
		Timestamp _last_update;
		int _seen;

		String unparse() const {
			Timestamp now = Timestamp::now();
			StringAccum sa;
			sa << _hna.unparse() << " ";
			sa << "seen " << _seen << " ";
			sa << "first_update " << now - _first_update << " ";
			sa << "last_update " << now - _last_update;
			return sa.take_string();
		}
	};

	typedef HashMap<HNAInfo, GWInfo> GWTable;
	typedef GWTable::const_iterator GWIter;

	Vector<HNAInfo> _hnas;
	GWTable _gateways;

	class DynGW *_dyn_gw;

	uint32_t _seq; // Next query sequence number to use.

	unsigned int _period; // msecs
	unsigned int _expire; // msecs

	Timer _timer;

	void start_ad(int);
	void send(WritablePacket *);
	void forward_seen(int, Seen *);

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
