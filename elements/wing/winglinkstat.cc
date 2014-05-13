/*
 * winglinkstat.{cc,hh} 
 * John Bicket, Roberto Riggio, Stefano Testi
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 * Copyright (c) 2009 CREATE-NET
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "winglinkstat.hh"
#include <click/args.hh>
#include <elements/wifi/availablerates.hh>
#include "winglinkmetric.hh"
CLICK_DECLS

enum {
	H_RESET, H_BCAST_STATS, H_BCAST_STATS_HT, H_NODE, H_TAU, H_PERIOD, H_PROBES, H_HT_PROBES, H_IFNAME
};

WINGLinkStat::WINGLinkStat() :
	_ads_rs_index(0), _neighbors_index(0), _neighbors_index_ht(0), 
	_tau(100000), _period(10000),
	_sent(0), _link_metric(0), _arp_table(0), _link_table(0),
	_timer(this), _debug(false) {
}

WINGLinkStat::~WINGLinkStat() {
}

void WINGLinkStat::run_timer(Timer *) {
	clear_stale();
	send_probe();
	int p = _period / _ads_rs.size();
	unsigned max_jitter = p / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	unsigned delay = p + j - max_jitter;
	_timer.reschedule_after_msec(delay);
}

int WINGLinkStat::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	_node = NodeAddress(_link_table->ip(), _ifid);
	reset();
	return 0;
}

int WINGLinkStat::configure(Vector<String> &conf, ErrorHandler *errh) {

	String probes;
	String ht_probes;

	if (Args(conf, this, errh)
		  .read_m("IFNAME", _ifname)
		  .read_m("ETH", _eth)
		  .read_m("IFID", _ifid)
		  .read_m("CHANNEL", _channel)
		  .read_m("RATES", ElementCastArg("AvailableRates"), _rtable)
		  .read_m("PROBES", probes)
		  .read_m("METRIC", ElementCastArg("WINGLinkMetric"), _link_metric)
		  .read_m("LT", ElementCastArg("LinkTableMulti"), _link_table)
		  .read_m("ARP", ElementCastArg("ARPTableMulti"), _arp_table)
		  .read("HT_RATES", ElementCastArg("AvailableRates"), _rtable_ht)
		  .read("HT_PROBES", ht_probes)
		  .read("PERIOD", _period)
		  .read("TAU", _tau)
		  .read("DEBUG", _debug)
		  .complete())
		return -1;

	write_handler(probes, this, (void *) H_PROBES, errh);
	write_handler(ht_probes, this, (void *) H_HT_PROBES, errh);

	return 0;

}

void WINGLinkStat::send_probe() {

	_arp_table->insert(_node, _eth);

	if (!_ads_rs.size()) {
		click_chatter("%{element} :: %s :: no probes to send at", this, __func__);
		return;
	}

	int size = _ads_rs[_ads_rs_index]._size;
	int rate = _ads_rs[_ads_rs_index]._rate;
	int rtype = _ads_rs[_ads_rs_index]._rtype;

	_ads_rs_index = (_ads_rs_index + 1) % _ads_rs.size();
	_sent++;
	unsigned min_packet_size = sizeof(click_ether) + sizeof(struct wing_probe);

	if ((unsigned) size < min_packet_size) {
		click_chatter("%{element} :: %s :: cannot send packet size %d: min is %d",
				this, 
				__func__, 
				size, 
				min_packet_size);
		return;
	}

	WritablePacket *p = Packet::make(size + sizeof(click_ether));
	if (!p) {
		click_chatter("%{element} :: %s :: cannot make probe packet!", this, __func__);
		return;
	}

	memset(p->data(), 0, p->length());
	click_ether *eh = (click_ether *) p->data();
	memset(eh->ether_dhost, 0xff, 6);
	memcpy(eh->ether_shost, _eth.data(), 6);

	wing_probe *lp = (struct wing_probe *) (p->data() + sizeof(click_ether));

	lp->_type = WING_PT_PROBE;
	lp->set_node(_node);
	lp->set_channel(_channel);
	lp->set_period(_period);
	lp->set_seq(Timestamp::now().sec());
	lp->set_tau(_tau);
	lp->set_sent(_sent);
	lp->set_rate(rate);
	lp->set_size(size);
	lp->set_rtype(rtype);
	lp->set_num_probes(_ads_rs.size());

	uint8_t *ptr = (uint8_t *) (lp + 1);
	uint8_t *end = (uint8_t *) p->data() + p->length();

	// rates entry
	Vector<int> rates;
	if (rtype == PROBE_TYPE_HT) {
		rates = _rtable_ht->lookup(_eth);
	} else {
		rates = _rtable->lookup(_eth);
	}
	if (rates.size() && ptr + sizeof(rate_entry) * rates.size() < end) {
		for (int x = 0; x < rates.size(); x++) {
			rate_entry *r_entry = (struct rate_entry *) (ptr);
			r_entry->set_rate(rates[x]);
			ptr += sizeof(rate_entry);
		}
		lp->set_num_rates(rates.size());
	}

	// links_entry
	int num_entries = 0;
	// neighbors counter
	int num_neighbors = 0;
	while (ptr < end && num_entries < _neighbors.size() && num_neighbors < _neighbors.size()) {
		ProbeList *probe;
		if (rtype == PROBE_TYPE_HT) {
			_neighbors_index_ht = (_neighbors_index_ht + 1) % _neighbors.size();
			if (_neighbors_index_ht >= _neighbors.size()) {
				break;
			}
			probe = _bcast_stats_ht.findp(_neighbors[_neighbors_index_ht]);
		} else {
			_neighbors_index = (_neighbors_index + 1) % _neighbors.size();
			if (_neighbors_index >= _neighbors.size()) {
				break;
			}
			probe = _bcast_stats.findp(_neighbors[_neighbors_index]);
		}
		if (!probe) {
			if (_debug) {
				click_chatter("%{element} :: %s :: lookup for %s failed", 
						this,
						__func__, 
						_neighbors[_neighbors_index].unparse().c_str());	
			}
			num_neighbors++;
			continue;
		}
		int size = probe->_probe_types.size() * sizeof(link_info) + sizeof(link_entry);
		if (ptr + size > end) {
			break;
		}
		num_neighbors++;
		num_entries++;
		link_entry *entry = (struct link_entry *) (ptr);
		entry->set_node(probe->_node);
		entry->set_channel(probe->_channel);
		entry->set_num_rates(probe->_probe_types.size());
		entry->set_seq(probe->_seq);	
		if ((uint32_t) probe->_node._ip > (uint32_t) _node._ip) {
			entry->set_seq(lp->seq());
		}
		ptr += sizeof(link_entry);
		Vector<int> fwd;
		Vector<int> rev;
		for (int x = 0; x < probe->_probe_types.size(); x++) {
			RateSize rs = probe->_probe_types[x];
			link_info *lnfo = (struct link_info *) (ptr + x * sizeof(link_info));
			lnfo->set_size(rs._size);
			lnfo->set_rate(rs._rate);
			lnfo->set_fwd(probe->fwd_rate(rs._rate, rs._size));
			lnfo->set_rev(probe->rev_rate(_start, rs._rate, rs._size));
			fwd.push_back(lnfo->fwd());
			rev.push_back(lnfo->rev());
		}
		ptr += probe->_probe_types.size() * sizeof(link_info);
	}

	lp->set_num_links(num_entries);

	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	ceh->magic = WIFI_EXTRA_MAGIC;
	ceh->rate = rate;

	checked_output_push(0, p);

}

Packet *
WINGLinkStat::simple_action(Packet *p) {
	click_ether *eh = (click_ether *) p->data();
	struct wing_probe *lp = (struct wing_probe *) (eh + 1);
	if (lp->_type != WING_PT_PROBE) {
		click_chatter("%{element} :: %s :: bad packet_type %04x", 
				this,
				__func__, 
				lp->_type);
		p->kill();
		return 0;
	}
	NodeAddress node = lp->node();
	if (node == _node) {
		click_chatter("%{element} :: %s :: got own packet %s", 
				this,
				__func__, 
				node.unparse().c_str());
		p->kill();
		return 0;
	}
	_arp_table->insert(node, EtherAddress(eh->ether_shost));
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	int rate = ceh->rate;
	if (rate != lp->rate()) {
		click_chatter("%{element} :: %s :: packet coming from %s says rate %d is %d", 
				this,
				__func__, 
				lp->node().unparse().c_str(), 
				lp->rate(), 
				rate);
		p->kill();
		return 0;
	}
	if (ceh->channel != lp->channel()) {
		click_chatter("%{element} :: %s :: packet coming from %s says channel %d is %d", 
				this,
				__func__, 
				lp->node().unparse().c_str(), 
				lp->channel(), 
				ceh->channel);
		p->kill();
		return 0;
	}
	uint32_t period = lp->period();
	uint32_t tau = lp->tau();
	ProbeList *probe_list;
	if (lp->rtype() == PROBE_TYPE_HT) {
		probe_list = _bcast_stats_ht.findp(node);
	} else {
		probe_list = _bcast_stats.findp(node);
	}
	if (!probe_list) {
		if (lp->rtype() == PROBE_TYPE_HT) {
			_bcast_stats_ht.insert(node, ProbeList(node, period, tau));
			probe_list = _bcast_stats_ht.findp(node);
		} else {
			_bcast_stats.insert(node, ProbeList(node, period, tau));
			probe_list = _bcast_stats.findp(node);
		}
		_neighbors.push_back(node);
		probe_list->_sent = 0;
	} else if (probe_list->_period != period) {
		if (_debug) {
			click_chatter("%{element} :: %s :: %s has changed its link probe period from %u to %u; clearing probe info", 
					this, 
					__func__, 
					node.unparse().c_str(), 
					probe_list->_period, 
					period);
		}
		probe_list->_probes.clear();
	} else if (probe_list->_tau != tau) {
		if (_debug) {
			click_chatter("%{element} :: %s :: %s has changed its link probe period from %u to %u; clearing probe info", 
					this, 
					__func__, 
					node.unparse().c_str(), 
					probe_list->_tau, 
					tau);
		}
		probe_list->_probes.clear();
	} else if (probe_list->_sent > lp->sent()) {
		if (_debug) {
			click_chatter("%{element} :: %s :: %s has reset; clearing probe info",
					this, 
					__func__, 
					node.unparse().c_str());
		}
		probe_list->_probes.clear();
	}

	Timestamp now = Timestamp::now();
	RateSize rs = RateSize(rate, lp->size(), lp->rtype());
	probe_list->_channel= lp->channel();
	probe_list->_period = lp->period();
	probe_list->_tau = lp->tau();
	probe_list->_sent = lp->sent();
	probe_list->_last_rx = now;
	probe_list->_num_probes = lp->num_probes();
	probe_list->_probes.push_back(Probe(now, lp->rate(), lp->size(), lp->seq(), ceh->rssi, ceh->silence));
	probe_list->_seq = lp->seq();
	uint32_t window = 1 + (probe_list->_tau / 1000);

	/* keep stats for at least the averaging period */
	while (probe_list->_probes.size() && now.sec() - probe_list->_probes[0]._when.sec() > (signed) window) {
		probe_list->_probes.pop_front();
	}

	int x = 0;
	for (x = 0; x < probe_list->_probe_types.size(); x++) {
		if (rs == probe_list->_probe_types[x]) {
			break;
		}
	}
	if (x == probe_list->_probe_types.size()) {
		probe_list->_probe_types.push_back(rs);
		probe_list->_fwd_rates.push_back(0);
	}
	uint8_t *ptr = (uint8_t *) (lp + 1);
	uint8_t *end = (uint8_t *) p->data() + p->length();

	// rates
	int num_rates = lp->num_rates();
	Vector<int> rates;
	for (int x = 0; x < num_rates; x++) {
		rate_entry *r_entry = (struct rate_entry *) (ptr);
		rates.push_back(r_entry->rate());
		ptr += sizeof(rate_entry);
	}

	if (lp->rtype() == PROBE_TYPE_HT) {
		_rtable_ht->insert(EtherAddress(eh->ether_shost), rates);
	} else {
		_rtable->insert(EtherAddress(eh->ether_shost), rates);
	}

	// links
	int link_number = 0;
	while (ptr < end && link_number < lp->num_links()) {
		link_number++;
		link_entry *entry = (struct link_entry *) (ptr);
		NodeAddress neighbor = entry->node();
		ptr += sizeof(struct link_entry);
		Vector<RateSize> rates;
		Vector<int> fwd;
		Vector<int> rev;
		for (uint8_t x = 0; x < entry->num_rates(); x++) {
			struct link_info *nfo = (struct link_info *) (ptr + x * (sizeof(struct link_info)));
			uint16_t nfo_size = nfo->size();
			uint16_t nfo_rate = nfo->rate();
			uint16_t nfo_fwd = nfo->fwd();
			uint16_t nfo_rev = nfo->rev();
			RateSize rs = RateSize(nfo_rate, nfo_size, lp->rtype());
			/* update other link stuff */
			rates.push_back(rs);
			rev.push_back(nfo_rev);
			if (neighbor == _node) {
				fwd.push_back(probe_list->rev_rate(_start, rates[x]._rate, rates[x]._size));
			} else {
				fwd.push_back(nfo_fwd);
			}
			if (neighbor == _node) {
				/* set the fwd rate */
				for (int x = 0; x < probe_list->_probe_types.size(); x++) {
					if (rs == probe_list->_probe_types[x]) {
						probe_list->_fwd_rates[x] = nfo_rev;
						break;
					}
				}
			}
		}

		_link_metric->update_link(node, neighbor, rates, fwd, rev, entry->seq(), entry->channel());
		ptr += entry->num_rates() * sizeof(struct link_info);
	}

	_link_table->dijkstra(true);
	_link_table->dijkstra(false);
	p->kill();

	return 0;
}

void WINGLinkStat::clear_stale()  {
	Vector<NodeAddress> new_neighbors;
	Timestamp now = Timestamp::now();
	ProbeList *list;
	for (int x = 0; x < _neighbors.size(); x++) {
		NodeAddress node = _neighbors[x];
		bool remove = false;
		// clear legacy stats
		list = _bcast_stats.findp(node);
		if (list && (unsigned) now.sec() - list->_last_rx.sec() > 2 * list->_period / 1000) {
			if (_debug) {
				click_chatter("%{element} :: %s :: clearing stale neighbor %s age %u", 
						this, 
						__func__, 
						node.unparse().c_str(),
						now.sec() - list->_last_rx.sec());
			}
			_bcast_stats.remove(node);
			remove = true;
		}
		// clear ht stats
		list = _bcast_stats_ht.findp(node);
		if (list && (unsigned) now.sec() - list->_last_rx.sec() > 2 * list->_period / 1000) {
			if (_debug) {
				click_chatter("%{element} :: %s :: clearing stale neighbor %s age %u", 
						this, 
						__func__, 
						node.unparse().c_str(),
						now.sec() - list->_last_rx.sec());
			}
			_bcast_stats_ht.remove(node);
			remove = true;
		} 
		// keep the nighbor
		if (!remove) {
			new_neighbors.push_back(node);
		}
	}
	_neighbors.clear();
	for (int x = 0; x < new_neighbors.size(); x++) {
		_neighbors.push_back(new_neighbors[x]);
	}
}

void WINGLinkStat::reset() {
	_neighbors.clear();
	_bcast_stats.clear();
	_bcast_stats_ht.clear();
	_sent = 0;
	_seq = 0;
	_start = Timestamp::now();
}

static int nodeaddress_sorter(const void *va, const void *vb, void *) {
	NodeAddress *a = (NodeAddress *) va, *b = (NodeAddress *) vb;
	if (a->addr() == b->addr()) {
		return 0;
	}
	return (ntohl(a->addr()) < ntohl(b->addr())) ? -1 : 1;
}

String WINGLinkStat::print_bcast_stats() {
	return print_stats(_bcast_stats, PROBE_TYPE_LEGACY);
}

String WINGLinkStat::print_bcast_stats_ht() {
	return print_stats(_bcast_stats_ht, PROBE_TYPE_HT);
}

String WINGLinkStat::print_stats(ProbeMap &stats, int type) {
	Vector<NodeAddress> addrs;
	for (ProbeIter iter = stats.begin(); iter.live(); iter++) {
		addrs.push_back(iter.key());
	}
	Timestamp now = Timestamp::now();
	StringAccum sa;
	click_qsort(addrs.begin(), addrs.size(), sizeof(NodeAddress), nodeaddress_sorter);
	for (int i = 0; i < addrs.size(); i++) {
		NodeAddress node = addrs[i];
		ProbeList *pl = stats.findp(node);
		sa << node.unparse().c_str();
		EtherAddress eth_dest = _arp_table->lookup(node);
		if (eth_dest && !eth_dest.is_broadcast()) {
			sa << " " << eth_dest.unparse().c_str();
		} else {
			sa << " ?";
		}
		sa << " seq " << pl->_seq;
		sa << " period " << pl->_period;
		sa << " tau " << pl->_tau;
		sa << " sent " << pl->_sent;
		sa << " last_rx " << now - pl->_last_rx;
		sa << "\n";
		for (int x = 0; x < _ads_rs.size(); x++) {
			int rtype = _ads_rs[x]._rtype;
			if (rtype != type) {
				continue;
			}
			int rate = _ads_rs[x]._rate;
			int size = _ads_rs[x]._size;
			int rev = pl->rev_rate(_start, rate, size);
			int fwd = pl->fwd_rate(rate, size);
			int rssi = pl->rev_rssi(rate, size);
			int noise = pl->rev_noise(rate, size);
			sa << node.unparse().c_str();
			EtherAddress eth_dest = _arp_table->lookup(node);
			if (eth_dest && !eth_dest.is_broadcast()) {
				sa << " " << eth_dest.unparse();
			} else {
				sa << " ?";
			}
			sa << " [ " << rate << " " << size << " ";
			sa << fwd << " " << rev << " ";
			sa << rssi << " " << noise << " ]";
			sa << "\n";
		}
	}
	return sa.take_string();
}

String WINGLinkStat::read_handler(Element *e, void *thunk) {
	WINGLinkStat *td = (WINGLinkStat *) e;
	switch ((uintptr_t) thunk) {
	case H_BCAST_STATS:
		return td->print_bcast_stats();
	case H_BCAST_STATS_HT:
		return td->print_bcast_stats_ht();
	case H_NODE:
		return td->_node.unparse() + "\n";
	case H_TAU:
		return String(td->_tau) + "\n";
	case H_IFNAME:
		return td->_ifname + "\n";
	case H_PERIOD:
		return String(td->_period) + "\n";
	case H_PROBES: {
		StringAccum sa;
		for (int x = 0; x < td->_ads_rs.size(); x++) {
			sa << td->_ads_rs[x]._rate << " " << td->_ads_rs[x]._size << " ";
		}
		return sa.take_string() + "\n";
	}
	default:
		return String() + "\n";
	}
}

int WINGLinkStat::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	WINGLinkStat *f = (WINGLinkStat *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_RESET: {
		f->reset();
		break;
	}
	case H_TAU: {
		unsigned m;
		if (!cp_unsigned(s, &m))
			return errh->error("tau parameter must be unsigned");
		f->_tau = m;
		f->reset();
		break;
	}
	case H_PERIOD: {
		unsigned m;
		if (!cp_unsigned(s, &m))
			return errh->error("period parameter must be unsigned");
		f->_period = m;
		f->reset();
		break;
	}
	case H_PROBES: {
		Vector<String> a;
		cp_spacevec(s, a);
		if (a.size() % 2 != 0) {
			return errh->error("must provide even number of numbers\n");
		}
		for (int x = 0; x < a.size() - 1; x += 2) {
			int rate;
			int size;
			if (!cp_integer(a[x], &rate)) {
				return errh->error("invalid PROBES rate value\n");
			}
			if (!cp_integer(a[x + 1], &size)) {
				return errh->error("invalid PROBES size value\n");
			}
			f->_ads_rs.push_back(RateSize(rate, size, PROBE_TYPE_LEGACY));
		}
		break;
	}
	case H_HT_PROBES: {
		Vector<String> a;
		cp_spacevec(s, a);
		if (a.size() % 2 != 0) {
			return errh->error("must provide even number of numbers\n");
		}
		for (int x = 0; x < a.size() - 1; x += 2) {
			int rate;
			int size;
			if (!cp_integer(a[x], &rate)) {
				return errh->error("invalid PROBES rate value\n");
			}
			if (!cp_integer(a[x + 1], &size)) {
				return errh->error("invalid PROBES size value\n");
			}
			f->_ads_rs.push_back(RateSize(rate, size, PROBE_TYPE_HT));
		}
		break;
	}
	}
	return 0;
}

void WINGLinkStat::add_handlers() {
	add_read_handler("bcast_stats", read_handler, (void *) H_BCAST_STATS);
	add_read_handler("bcast_stats_ht", read_handler, (void *) H_BCAST_STATS_HT);
	add_read_handler("node", read_handler, (void *) H_NODE);
	add_read_handler("tau", read_handler, (void *) H_TAU);
	add_read_handler("ifname", read_handler, (void *) H_IFNAME);
	add_read_handler("period", read_handler, (void *) H_PERIOD);
	add_read_handler("probes", read_handler, (void *) H_PROBES);
	add_write_handler("reset", write_handler, (void *) H_RESET);
	add_write_handler("tau", write_handler, (void *) H_TAU);
	add_write_handler("period", write_handler, (void *) H_PERIOD);
	add_write_handler("probes", write_handler, (void *) H_PROBES);
	add_write_handler("ht_probes", write_handler, (void *) H_HT_PROBES);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ARPTableMulti AvailableRates)
EXPORT_ELEMENT(WINGLinkStat)

