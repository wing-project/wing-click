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
#include <click/error.hh>
#include <click/ipaddress.hh>
#include "winglinkstat.hh"
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <elements/wifi/availablerates.hh>
#include "devinfo.hh"
#include "arptablemulti.hh"
#include "availablechannels.hh"
#include "winglinkmetric.hh"
#include "linktablemulti.hh"
#include "wingpacket.hh"
CLICK_DECLS

enum {
	H_RESET, H_BCAST_STATS, H_NODE, H_TAU, H_PERIOD, H_PROBES
};

WINGLinkStat::WINGLinkStat() :
	_ads_rs_index(0), _neighbors_index(0), _tau(100000), _period(10000),
	_sent(0), _link_metric(0), _arp_table(0), _link_table(0),
	_dev(0), _timer(this), _debug(false) {
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

int WINGLinkStat::initialize(ErrorHandler *errh) {
	if (noutputs() > 0) {
		int p = _period / _ads_rs.size();
		unsigned max_jitter = p / 10;
		unsigned j = click_random(0, 2 * max_jitter);
		_timer.initialize(this);
		_timer.reschedule_after_msec(p + j - max_jitter);
	}
	int iface = _link_table->reverse_lookup(_dev->eth());
	if (iface == 0) {
		return errh->error("Unable to fetch indentifier for interface %s", _dev->eth().unparse().c_str());
	}
	_node._iface = iface;
	reset();
	return 0;
}

int WINGLinkStat::configure(Vector<String> &conf, ErrorHandler *errh) {

	String probes;

	if (cp_va_kparse(conf, this, errh, 
			"IP", cpkM, cpIPAddress, &_node,
			"DEV", cpkM, cpElementCast, "DevInfo", &_dev,
			"METRIC", cpkM, cpElementCast, "WINGLinkMetric", &_link_metric, 
			"ARP", cpkM, cpElementCast, "ARPTableMulti", &_arp_table, 
			"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
			"PROBES", cpkM, cpString, &probes, 
			"PERIOD", 0, cpUnsigned, &_period, 
			"TAU", 0, cpUnsigned, &_tau, 
			"DEBUG", 0, cpBool, &_debug, 
			cpEnd) < 0)
		return -1;

	return write_handler(probes, this, (void *) H_PROBES, errh);

}

void WINGLinkStat::send_probe() {

	_arp_table->insert(_node, _dev->eth());

	if (!_ads_rs.size()) {
		click_chatter("%{element} :: %s :: no probes to send at", this, __func__);
		return;
	}

	int size = _ads_rs[_ads_rs_index]._size;
	int rate = _ads_rs[_ads_rs_index]._rate;

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
	memcpy(eh->ether_shost, _dev->eth().data(), 6);

	wing_probe *lp = (struct wing_probe *) (p->data() + sizeof(click_ether));

	lp->_type = WING_PT_PROBE;
	lp->set_node(_node);
	lp->set_channel(_dev->channel());
	lp->set_period(_period);
	lp->set_seq(Timestamp::now().sec());
	lp->set_tau(_tau);
	lp->set_sent(_sent);
	lp->unset_flag(~0);
	lp->set_rate(rate);
	lp->set_size(size);
	lp->set_num_probes(_ads_rs.size());

	uint8_t *ptr = (uint8_t *) (lp + 1);
	uint8_t *end = (uint8_t *) p->data() + p->length();

	// rates entry
	Vector<int> rates = ((AvailableRates*)_dev->rtable())->lookup(_dev->eth());
	if (rates.size() && ptr + sizeof(rate_entry) * rates.size() < end) {
		for (int x = 0; x < rates.size(); x++) {
			rate_entry *r_entry = (struct rate_entry *) (ptr);
			r_entry->set_rate(rates[x]);
			ptr += sizeof(rate_entry);
		}
		lp->set_flag(PROBE_FLAGS_RATES);
		lp->set_num_rates(rates.size());
	}

	// channels entry
	Vector<int> channels = ((AvailableChannels*)_dev->ctable())->lookup(_dev->eth());
	if (channels.size() && ptr + sizeof(channel_entry) * channels.size() < end) {
	  for (int x = 0; x < channels.size(); x++) {
			channel_entry *c_entry = (struct channel_entry *) (ptr);
			c_entry->set_channel(channels[x]);
			ptr += sizeof(channel_entry);
		}
		lp->set_flag(PROBE_FLAGS_CHANNELS);
		lp->set_num_channels(channels.size());
	}

	// links_entry
	int num_entries = 0;
	while (ptr < end && num_entries < _neighbors.size()) {
		_neighbors_index = (_neighbors_index + 1) % _neighbors.size();
		if (_neighbors_index >= _neighbors.size()) {
			break;
		}
		ProbeList *probe = _bcast_stats.findp(_neighbors[_neighbors_index]);
		if (!probe) {
			click_chatter("%{element} :: %s :: lookup for %s, %d failed", 
					this,
					__func__, 
					_neighbors[_neighbors_index].unparse().c_str(),
					_neighbors_index);
			continue;
		}
		int size = probe->_probe_types.size() * sizeof(link_info) + sizeof(link_entry);
		if (ptr + size > end) {
			break;
		}
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
		Vector<RateSize> rates;
		Vector<int> fwd;
		Vector<int> rev;
		for (int x = 0; x < probe->_probe_types.size(); x++) {
			RateSize rs = probe->_probe_types[x];
			link_info *lnfo = (struct link_info *) (ptr + x * sizeof(link_info));
			lnfo->set_size(rs._size);
			lnfo->set_rate(rs._rate);
			lnfo->set_fwd(probe->fwd_rate(rs._rate, rs._size));
			lnfo->set_rev(probe->rev_rate(_start, rs._rate, rs._size));
			rates.push_back(rs);
			fwd.push_back(lnfo->fwd());
			rev.push_back(lnfo->rev());
		}
		ptr += probe->_probe_types.size() * sizeof(link_info);
	}

	lp->set_flag(PROBE_FLAGS_LINKS);
	lp->set_num_links(num_entries);

	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	ceh->magic = WIFI_EXTRA_MAGIC;
	ceh->rate = rate;
	ceh->max_tries = WIFI_MAX_RETRIES + 1;

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
	if (ceh->rate != lp->rate()) {
		click_chatter("%{element} :: %s :: packet comping from %s says rate %d is %d", 
				this,
				__func__, 
				lp->node().unparse().c_str(), 
				lp->rate(), 
				ceh->rate);
		p->kill();
		return 0;
	}
	if (ceh->channel != lp->channel()) {
		click_chatter("%{element} :: %s :: packet comping from %s says channel %d is %d", 
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
	ProbeList *probe_list = _bcast_stats.findp(node);
	if (!probe_list) {
		_bcast_stats.insert(node, ProbeList(node, period, tau));
		_neighbors.push_back(node);
		probe_list = _bcast_stats.findp(node);
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
	RateSize rs = RateSize(ceh->rate, lp->size());
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
	if (lp->flag(PROBE_FLAGS_RATES)) {
		int num_rates = lp->num_rates();
		Vector<int> rates;
		for (int x = 0; x < num_rates; x++) {
			rate_entry *r_entry = (struct rate_entry *) (ptr);
			rates.push_back(r_entry->rate());
			ptr += sizeof(rate_entry);
		}
		((AvailableRates *)_dev->rtable())->insert(EtherAddress(eh->ether_shost), rates);
	}
	// channels
	if (lp->flag(PROBE_FLAGS_CHANNELS)) {
		int num_channels = lp->num_channels();
		Vector<int> channels;
		for (int x = 0; x < num_channels; x++) {
			channel_entry *c_entry = (struct channel_entry *) (ptr);
			channels.push_back(c_entry->channel());
			ptr += sizeof(channel_entry);
		}
		((AvailableChannels *)_dev->ctable())->insert(EtherAddress(eh->ether_shost), channels);
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
			RateSize rs = RateSize(nfo_rate, nfo_size);
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
	p->kill();
	return 0;
}

void WINGLinkStat::clear_stale()  {
	Vector<NodeAddress> new_neighbors;
	Timestamp now = Timestamp::now();
	for (int x = 0; x < _neighbors.size(); x++) {
		NodeAddress node = _neighbors[x];
		ProbeList *list = _bcast_stats.findp(node);
		if (!list || (unsigned) now.sec() - list->_last_rx.sec() > 2 * list->_period / 1000) {
			if (_debug) {
				click_chatter("%{element} :: %s :: clearing stale neighbor %s age %u", 
						this, 
						__func__, 
						node.unparse().c_str(),
						now.sec() - list->_last_rx.sec());
			}
			_bcast_stats.remove(node);
		} else {
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
	Vector<NodeAddress> addrs;
	for (ProbeIter iter = _bcast_stats.begin(); iter.live(); iter++) {
		addrs.push_back(iter.key());
	}
	Timestamp now = Timestamp::now();
	StringAccum sa;
	click_qsort(addrs.begin(), addrs.size(), sizeof(NodeAddress), nodeaddress_sorter);
	for (int i = 0; i < addrs.size(); i++) {
		NodeAddress node = addrs[i];
		ProbeList *pl = _bcast_stats.findp(node);
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
	case H_NODE:
		return td->_node.unparse() + "\n";
	case H_TAU:
		return String(td->_tau) + "\n";
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
	}
	case H_PERIOD: {
		unsigned m;
		if (!cp_unsigned(s, &m))
			return errh->error("period parameter must be unsigned");
		f->_period = m;
		f->reset();
	}
	case H_PROBES: {
		Vector<RateSize> ads_rs;
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
			ads_rs.push_back(RateSize(rate, size));
		}
		if (!ads_rs.size()) {
			return errh->error("no PROBES provided\n");
		}
		f->_ads_rs = ads_rs;
	}
	}
	return 0;
}

void WINGLinkStat::add_handlers() {
	add_read_handler("bcast_stats", read_handler, (void *) H_BCAST_STATS);
	add_read_handler("node", read_handler, (void *) H_NODE);
	add_read_handler("tau", read_handler, (void *) H_TAU);
	add_read_handler("period", read_handler, (void *) H_PERIOD);
	add_read_handler("probes", read_handler, (void *) H_PROBES);
	add_write_handler("reset", write_handler, (void *) H_RESET);
	add_write_handler("tau", write_handler, (void *) H_TAU);
	add_write_handler("period", write_handler, (void *) H_PERIOD);
	add_write_handler("probes", write_handler, (void *) H_PROBES);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ARPTableMulti AvailableChannels AvailableRates)
EXPORT_ELEMENT(WINGLinkStat)

