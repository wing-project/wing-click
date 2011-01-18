#ifndef CLICK_WINGBASE_HH
#define CLICK_WINGBASE_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include "winglinkstat.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
#include "arptablemulti.hh"
#include "wingpacket.hh"
CLICK_DECLS

template <typename T>
class WINGBase: public Element {
public:

	WINGBase();
	~WINGBase();

	const char *class_name() const { return "WINGBase"; }
	const char *processing() const { return AGNOSTIC; }

	/* handler stuff */
	void add_handlers();

protected:

	// List of reply sequence #s that we've already seen.
	class Seen {
	public:
		Seen() : 
			_seen(T()),
			_seq(0),
			_count(0),
			_forwarded(false) {
		}
		Seen(T seen, int seq) : _seen(seen), _seq(seq), _count(0), _when(Timestamp::now()) {}
		T _seen;
		int _seq;
		int _count;
		Timestamp _when;
		Timestamp _to_send;
		bool _forwarded;
	};

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	DEQueue<Seen> _seen;

	unsigned int _jitter; // msecs
	int _max_seen_size; 
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

	bool update_link_table(Packet *);
	Packet * create_wing_packet(NodeAddress, NodeAddress, int, IPAddress, IPAddress, IPAddress, int, PathMulti, int);

	virtual void forward_seen(int, Seen *) = 0;

	bool process_seen(T, int, bool);
	void append_seen(T, int);
	void forward_seen_hook();

	static void static_forward_seen_hook(Timer *t, void *e) {
		delete t; ((WINGBase *) e)->forward_seen_hook();
	}

};

template <typename T>
WINGBase<T>::WINGBase() :
	_link_table(0), _arp_table(0), _jitter(1000), _max_seen_size(100), _debug(false) {
}

template <typename T>
WINGBase<T>::~WINGBase() {
}

template <typename T>
void
WINGBase<T>::forward_seen_hook() {
	Timestamp now = Timestamp::now();
	Vector<int> ifs = _link_table->get_local_interfaces();
	_link_table->dijkstra(false);
	for (int x = 0; x < _seen.size(); x++) {
		if (_seen[x]._to_send < now && !_seen[x]._forwarded) {
			for (int i = 0; i < ifs.size(); i++) {	
				forward_seen(ifs[i], &_seen[x]);
			}
			_seen[x]._forwarded = true;
		}
	}
}

template <typename T>
void
WINGBase<T>::append_seen(T seen, int seq) {
	if (_seen.size() >= _max_seen_size) {
		_seen.pop_front();
	}
	_seen.push_back(Seen(seen, seq));
}

template <typename T>
bool
WINGBase<T>::process_seen(T seen, int seq, bool schedule) {
	int si = 0;
	for (si = 0; si < _seen.size(); si++) {
		if (seen == _seen[si]._seen && seq == _seen[si]._seq) {
			_seen[si]._count++;
			return false;
		}
	}
	if (si == _seen.size()) {
		if (_seen.size() >= _max_seen_size) {
			_seen.pop_front();
			si--;
		}
		_seen.push_back(Seen(seen, seq));
	}		
	_seen[si]._count++;
	_seen[si]._when = Timestamp::now();
	/* schedule timer */
	if (schedule) {
		int delay = click_random(1, _jitter);
		_seen[si]._to_send = _seen[si]._when + Timestamp::make_msec(delay);
		_seen[si]._forwarded = false;
		Timer *t = new Timer(static_forward_seen_hook, (void *) this);
		t->initialize(this);
		t->schedule_after_msec(delay);
	}
	return true;
}

template <typename T>
Packet *
WINGBase<T>::create_wing_packet(NodeAddress src, NodeAddress dst, int type, IPAddress qdst, IPAddress netmask, IPAddress qsrc, int seq, PathMulti best, int next) {

	if (!src) {
		click_chatter("%{element} :: %s :: bad source address",
			      this,
			      __func__);
		return 0;
	}

	if ((best.size() > 0) && !_link_table->valid_route(best)) {
		click_chatter("%{element} :: %s :: invalid route %s", 
				this,
				__func__, 
				route_to_string(best).c_str());
		return 0;
	}

	int hops = (best.size() > 0) ? best.size() - 1 : 0;
	int len = wing_packet::len_wo_data(hops);

	WritablePacket *p = Packet::make(len + sizeof(click_ether));

	if (p == 0) {
		click_chatter("%{element} :: %s :: cannot make packet!", this, __func__);
		return 0;
	}

	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	memset(pk, '\0', len);

	pk->_type = type;
	pk->set_qdst(qdst);
	pk->set_netmask(netmask);
	pk->set_qsrc(qsrc);
	pk->set_seq(seq);
	pk->set_next(next);
	pk->set_num_links(hops);

	for (int i = 0; i < hops; i++) {
		pk->set_link(i, best[i].dep(), best[i + 1].arr(), 
				_link_table->get_link_metric(best[i].dep(), best[i + 1].arr()), 
				_link_table->get_link_metric(best[i + 1].arr(), best[i].dep()), 
				_link_table->get_link_seq(best[i].dep(), best[i + 1].arr()), 
				_link_table->get_link_age(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_channel(best[i].dep(), best[i + 1].arr()));
	}

	EtherAddress eth_src = _arp_table->lookup(src);
	if (src && eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				src.unparse().c_str(),
				eth_src.unparse().c_str());
	}

	EtherAddress eth_dst = _arp_table->lookup(dst);
	if (dst && eth_dst.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for dst %s (%s)", 
				this,
				__func__,
				dst.unparse().c_str(),
				eth_dst.unparse().c_str());

	}

	memcpy(eh->ether_dhost, eth_dst.data(), 6);
	memcpy(eh->ether_shost, eth_src.data(), 6);

	return p;

}

template <typename T>
bool
WINGBase<T>::update_link_table(Packet *p) {
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	/* update the metrics from the packet */
	for (int i = 0; i < pk->num_links(); i++) {
		NodeAddress a = pk->get_link_dep(i);
		NodeAddress b = pk->get_link_arr(i);
		uint32_t fwd_m = pk->get_link_fwd(i);
		uint32_t rev_m = pk->get_link_rev(i);
		uint32_t seq = pk->get_link_seq(i);
		uint32_t age = pk->get_link_age(i);
		uint32_t channel = pk->get_link_channel(i);
		if (!fwd_m || !rev_m || !seq || !channel) {
			click_chatter("%{element} :: %s :: invalid link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					rev_m,
					seq,
					channel,
					b.unparse().c_str());
			return false;
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					seq,
					age,
					fwd_m,
					channel,
					b.unparse().c_str());
		}
		if (fwd_m && !_link_table->update_link(a, b, seq, age, fwd_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update fwd_m %s > %d > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					b.unparse().c_str());
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					b.unparse().c_str(), 
					seq,
					age,
					rev_m,
					channel,
					a.unparse().c_str());
		}
		if (rev_m && !_link_table->update_link(b, a, seq, age, rev_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update rev_m %s < %d < %s",
					this, 
					__func__, 
					b.unparse().c_str(), 	
					rev_m,
					a.unparse().c_str());
		}
	}
	_link_table->dijkstra(true);
	_link_table->dijkstra(false);
	return true;
}

enum {
	H_BASE_DEBUG, H_BASE_IP
};

template <typename T>
String
WINGBase<T>::read_handler(Element *e, void *thunk) {
	WINGBase<T> *td = (WINGBase<T> *) e;
	switch ((uintptr_t) thunk) {
	case H_BASE_DEBUG:
		return String(td->_debug) + "\n";
	case H_BASE_IP:
		return td->_ip.unparse() + "\n";
	default:
		return String();
	}
}

template <typename T>
int
WINGBase<T>::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {
	WINGBase<T> *f = (WINGBase<T> *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_BASE_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

template <typename T>
void
WINGBase<T>::add_handlers() {
	add_read_handler("ip", read_handler, H_BASE_IP);
	add_read_handler("debug", read_handler, H_BASE_DEBUG);
	add_write_handler("debug", write_handler, H_BASE_DEBUG);
}

CLICK_ENDDECLS
#endif
