#ifndef CLICK_LINKTABLEMULTI_HH
#define CLICK_LINKTABLEMULTI_HH
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include "nodeaddress.hh"
#include "pathmulti.hh"
#include "devinfo.hh"
CLICK_DECLS

/*
 * =c
 * LinkTableMulti(IP Address, [STALE timeout])
 * =s Wifi
 * Keeps a Link state database and calculates Weighted Shortest Path
 * for other elements
 * =d
 * Runs dijkstra's algorithm occasionally.
 * =a ARPTable
 *
 */

typedef HashMap<uint16_t, uint32_t> MetricTable;
typedef HashMap<uint16_t, uint32_t>::const_iterator MetricIter;

class HostInfo {
public:
	IPAddress _ip;

	uint8_t _if_from_me;
	uint8_t _if_to_me;

	uint32_t _metric_from_me;
	uint32_t _metric_to_me;

	NodeAddress _prev_from_me;
	NodeAddress _prev_to_me;

	MetricTable _metric_table_from_me;
	MetricTable _metric_table_to_me;

	bool _marked_from_me;
	bool _marked_to_me;

	Vector<int> _interfaces;

	HostInfo(IPAddress ip) {
		_ip = ip;
		_if_from_me = 0;
		_if_to_me = 0;
		_metric_from_me = 0;
		_metric_to_me = 0;
		_prev_from_me = NodeAddress();
		_prev_to_me = NodeAddress();
		_marked_from_me = false;
		_marked_to_me = false;
	}

	HostInfo() {
		HostInfo(IPAddress());
	}

	HostInfo(const HostInfo &p) :
		_ip(p._ip), _if_from_me(p._if_from_me), _if_to_me(p._if_to_me),
		_metric_from_me(p._metric_from_me), _metric_to_me(p._metric_to_me), 
		_prev_from_me(p._prev_from_me), _prev_to_me(p._prev_to_me), 
		_marked_from_me(p._marked_from_me), _marked_to_me(p._marked_to_me) {
	}

	void clear(bool from_me) {
		if (from_me) {
			_prev_from_me = NodeAddress();
			_metric_from_me = 0;
			_marked_from_me = false;
		} else {
			_prev_to_me = NodeAddress();
			_metric_to_me = 0;
			_marked_to_me = false;
		}
	}

	void add_interface(uint8_t iface) {
		if (iface == 0) {
			return;
		}
		for (Vector<int>::iterator iter = _interfaces.begin(); iter != _interfaces.end(); iter ++){
			if (iface == *iter) {
				return;
			}
		}
		_interfaces.push_back(iface);
	}

};

class LinkInfo {
public:
	NodeAddress _from;
	NodeAddress _to;
	uint32_t _metric;
	uint32_t _seq;
	uint32_t _age;
	uint16_t _channel;
	Timestamp _last_updated;

	LinkInfo() {
		_from = NodeAddress();
		_to = NodeAddress();
		_metric = 0;
		_seq = 0;
		_age = 0;
		_channel = 0;
		_last_updated.assign_now();
	}

	LinkInfo(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel) {
		_from = from;
		_to = to;
		_metric = metric;
		_seq = seq;
		_age = age;
		_channel = channel;
		_last_updated.assign_now();
	}

	LinkInfo(const LinkInfo &p) :
		_from(p._from), _to(p._to), _metric(p._metric), _seq(p._seq), 
		_age(p._age), _channel(p._channel), _last_updated(p._last_updated) {
	}

	uint32_t age() {
		Timestamp now = Timestamp::now();
		return _age + (now.sec() - _last_updated.sec());
	}

	void update(uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel) {
		if (seq <= _seq) {
			return;
		}
		_metric = metric;
		_seq = seq;
		_age = age;
		_channel = channel;
		_last_updated.assign_now();
	}

	String unparse() {
		StringAccum sa;
		sa << _from.unparse() << " " << _to.unparse();
		sa << " " << _channel;
		sa << " " << _metric;
		sa << " " << _seq;
		sa << " " << age();
		return sa.take_string();
	}
};

class NodePair {
public:

	NodeAddress _to;
	NodeAddress _from;

	NodePair() :
		_to(), _from() {
	}

	NodePair(NodeAddress from, NodeAddress to) :
		_to(to), _from(from) {
	}

	bool contains(NodeAddress foo) const {
		return (foo == _to || foo == _from);
	}

	inline hashcode_t hashcode() const {
		return CLICK_NAME(hashcode)(_to) + CLICK_NAME(hashcode)(_from);
	}

	inline bool operator==(NodePair other) const {
		return (other._to == _to && other._from == _from);
	}

};

class LinkTableMulti: public Element {
public:

	LinkTableMulti();
	~LinkTableMulti();

	const char *class_name() const { return "LinkTableMulti"; }
	int initialize(ErrorHandler *);
	int configure(Vector<String> &conf, ErrorHandler *errh);

	void run_timer(Timer *);
	void take_state(Element *, ErrorHandler *);
	void *cast(const char *n);

	/* handler stuff */
	void add_handlers();

	/* read/write handlers */
	String print_routes(bool);
	String print_links();
	String print_hosts();

	inline Vector<int> get_local_interfaces() { 
		HostInfo *nfo = _hosts.findp(_ip);
		if (nfo) {
			return nfo->_interfaces;
		}
		return Vector<int>();
	}

	inline EtherAddress lookup(int interface) {
		DevInfo *nfo = _local_interfaces.find(interface);
		if (nfo) {
			return nfo->eth();
		}
		return EtherAddress();
	}

	inline int reverse_lookup(EtherAddress eth) {
		for (IIter iter = _local_interfaces.begin(); iter.live(); iter++) {
			DevInfo *nfo = iter.value();
			if (nfo->eth() == eth) {
				return iter.key();
			}
		}
		return 0;
	}

	void clear();

	/* other public functions */
	String route_to_string(PathMulti p);

	bool update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel);
	bool update_both_links(NodeAddress a, NodeAddress b, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel) {	
		if (update_link(a, b, seq, age, metric, channel)) {
			return update_link(b, a, seq, age, metric, channel);
		}
		return false;
	}
	bool update_both_links(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t age, uint32_t fwd, uint32_t rev, uint16_t channel) {	
		if (update_link(from, to, seq, age, fwd, channel)) {
			return update_link(to, from, seq, age, rev, channel);
		}
		return false;
	}

	uint32_t get_link_metric(NodeAddress from, NodeAddress to);
	uint32_t get_link_seq(NodeAddress from, NodeAddress to);
	uint32_t get_link_age(NodeAddress from, NodeAddress to);
	uint32_t get_link_channel(NodeAddress from, NodeAddress to);

	bool valid_route(const PathMulti &);
	uint32_t get_route_metric(const PathMulti &);
	Vector<NodeAddress> get_neighbors(NodeAddress);
	void dijkstra(bool);
	void clear_stale();
	PathMulti best_route(IPAddress dst, bool from_me);

	Vector<PathMulti> top_n_routes(IPAddress dst, int n);
	uint32_t get_host_metric_to_me(IPAddress s);
	uint32_t get_host_metric_from_me(IPAddress s);

	typedef HashMap<NodeAddress, NodeAddress> NodeTable;
	typedef NodeTable::const_iterator NodeIter;

	NodeTable _blacklist;
	Timestamp _dijkstra_time;

protected:

	typedef HashMap<IPAddress, HostInfo> HTable;
	typedef HTable::const_iterator HTIter;

	typedef HashMap<NodePair, LinkInfo> LTable;
	typedef LTable::const_iterator LTIter;

	typedef HashMap<int, DevInfo*> ITable;
	typedef ITable::const_iterator IIter;

	HTable _hosts;
	LTable _links;
	ITable _local_interfaces;

	IPAddress _ip;
	Timestamp _stale_timeout;
	Timer _timer;
	bool _wcett;
	uint32_t _beta;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif 
