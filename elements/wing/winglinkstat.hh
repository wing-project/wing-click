#ifndef CLICK_WINGLINKSTAT_HH
#define CLICK_WINGLINKSTAT_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

class RateSize {
public:
	RateSize(int rate, int size, int rtype) : _rate(rate), _size(size), _rtype(rtype) {}
	int _rate;
	int _size;
	int _rtype;
	inline bool operator==(RateSize other) {
		return (other._rate == _rate && other._size == _size && other._rtype == _rtype);
	}
};

class Probe {
public:
	Probe(const Timestamp &when, int rate, int size, uint32_t seq, uint32_t rssi, uint32_t noise) :
		_when(when), _rate(rate), _size(size), _seq(seq), _rssi(rssi), _noise(noise) {
	}
	Timestamp _when;
	int _rate;
	int _size;
	uint32_t _seq;
	uint32_t _rssi;
	uint32_t _noise;
};

class ProbeList {
public:
	ProbeList() :
		_period(0), _tau(0), _last_rx(0) {
	}
	ProbeList(const NodeAddress &node, uint32_t period, uint32_t tau) :
		_node(node), _period(period), _tau(tau), _sent(0), _last_rx(0) {
	}

	NodeAddress _node;
	uint32_t _period; // period of this node's probes, as reported by the node
	uint32_t _tau; // this node's stats averaging period, as reported by the node
	uint32_t _sent;
	uint32_t _num_probes;
	uint32_t _channel;
	uint32_t _seq;
	Vector<RateSize> _probe_types;
	Vector<int> _fwd_rates;
	Timestamp _last_rx;
	Deque<Probe> _probes; // most recently received probes

	int fwd_rate(int rate, int size) {
		if (Timestamp::now() - _last_rx > Timestamp::make_msec(_tau)) {
			return 0;
		}
		for (int x = 0; x < _probe_types.size(); x++) {
			if (_probe_types[x]._size == size && _probe_types[x]._rate == rate) {
				return _fwd_rates[x];
			}
		}
		return 0;
	}

	int rev_rate(const Timestamp &start, int rate, int size) {
		Timestamp now = Timestamp::now();
		Timestamp earliest = now - Timestamp::make_msec(_tau);
		if (_period == 0) {
			return 0;
		}
		int num = 0;
		for (int i = _probes.size() - 1; i >= 0; i--) {
			if (earliest > _probes[i]._when) {
				break;
			}
			if (_probes[i]._size == size && _probes[i]._rate == rate) {
				num++;
			}
		}
		Timestamp since_start = now - start;
		uint32_t ms_since_start = WIFI_MAX(0, since_start.msecval());
		uint32_t fake_tau = WIFI_MIN(_tau, ms_since_start);
		assert(_probe_types.size());
		uint32_t num_expected = (fake_tau / _period);
		if (_sent / _num_probes < num_expected) {
			num_expected = _sent / _num_probes;
		}
		if (!num_expected) {
			num_expected = 1;
		}
		return WIFI_MIN(100, 100 * num / num_expected);
	}

	int rev_rssi(int rate, int size) {
		Timestamp now = Timestamp::now();
		Timestamp earliest = now - Timestamp::make_msec(_tau);
		if (_period == 0) {
			return 0;
		}
		int num = 0;
		int sum = 0;
		for (int i = _probes.size() - 1; i >= 0; i--) {
			if (earliest > _probes[i]._when) {
				break;
			}
			if (_probes[i]._size == size && _probes[i]._rate == rate) {
				num++;
				sum += _probes[i]._rssi;
			}
		}
		if (!num) {
			return -1;
		}
		return (sum / num);
	}

	int rev_noise(int rate, int size) {
		Timestamp now = Timestamp::now();
		Timestamp earliest = now - Timestamp::make_msec(_tau);
		if (_period == 0) {
			return 0;
		}
		int num = 0;
		int sum = 0;
		for (int i = _probes.size() - 1; i >= 0; i--) {
			if (earliest > _probes[i]._when) {
				break;
			}
			if (_probes[i]._size == size && _probes[i]._rate == rate) {
				num++;
				sum += _probes[i]._noise;
			}
		}
		if (!num) {
			return -1;
		}
		return (sum / num);
	}
};

class WINGLinkStat: public Element {
public:

	WINGLinkStat();
	~WINGLinkStat();

	const char *class_name() const { return "WINGLinkStat"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);

	Packet *simple_action(Packet *);

	/* handler stuff */
	void add_handlers();
	String print_bcast_stats();
	String print_bcast_stats_ht();

private:

	Vector<RateSize> _ads_rs;
	int _ads_rs_index;

	Vector<NodeAddress> _neighbors;
	int _neighbors_index;
	int _neighbors_index_ht;

	typedef HashMap<NodeAddress, ProbeList> ProbeMap;
	typedef ProbeMap::const_iterator ProbeIter;

	ProbeMap _bcast_stats;
	ProbeMap _bcast_stats_ht;

	Timestamp _start;
	NodeAddress _node;

	uint32_t _tau; // msecs
	uint32_t _period; // msecs
	uint32_t _sent;
	uint32_t _seq;

	String _ifname;
	EtherAddress _eth;
	uint32_t _channel;
	uint32_t _ifid;

	class AvailableRates *_rtable;
	class AvailableRates *_rtable_ht;
	class WINGLinkMetric *_link_metric;
	class ARPTableMulti *_arp_table;
	class LinkTableMulti *_link_table;

	Timer _timer;
	bool _debug;

	void run_timer(Timer *);
	void reset();
	void clear_stale();
	void send_probe();

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);
	String print_stats(ProbeMap &, int);

};

CLICK_ENDDECLS
#endif

