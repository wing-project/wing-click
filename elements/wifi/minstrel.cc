/*
 * minstrel.{cc,hh} -- sets wifi txrate annotation on a packet
 * Roberto
 *
 * Copyright (c) 2010 CREATE-NET
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <elements/wifi/availablerates.hh>
#include "minstrel.hh"

CLICK_DECLS

Minstrel::Minstrel() 
  : _timer(this) {
}

Minstrel::~Minstrel() {
}

void Minstrel::run_timer(Timer *)
{
	for (NeighborIter iter = _neighbors.begin(); iter.live(); iter++) {
		DstInfo *nfo = &iter.value();
		int max_tp = 0, index_max_tp = 0, index_max_tp2 = 0;
		int max_prob = 0, index_max_prob = 0;
		uint32_t usecs;
		int i;
		uint32_t p;
		for (i = 0; i < nfo->_rates.size(); i++) {
			usecs = calc_usecs_wifi_packet(1500, nfo->_rates[i], 0);
			if (!usecs) {
				usecs = 1000000;
			}
			/* To avoid rounding issues, probabilities scale from 0 (0%)
			 * to 18000 (100%) */
			if (nfo->_attempts[i]) {
				p = (nfo->_success[i] * 18000) / nfo->_attempts[i];
				nfo->_hist_success[i] += nfo->_success[i];
				nfo->_hist_attempts[i] += nfo->_attempts[i];
				nfo->_cur_prob[i] = p;
				p = ((p * (100 - _ewma_level)) + (nfo->_probability[i] * _ewma_level)) / 100;
				nfo->_probability[i] = p;
				nfo->_cur_tp[i] = p * (1000000 / usecs);

			}
			nfo->_last_success[i] = nfo->_success[i];
			nfo->_last_attempts[i] = nfo->_attempts[i];
			nfo->_success[i] = 0;
			nfo->_attempts[i] = 0;
			/* Sample less often below the 10% chance of success.
			 * Sample less often above the 95% chance of success. */
			if ((nfo->_probability[i] > 17100) || (nfo->_probability[i] < 1800)) {
				nfo->_sample_limit[i] = 4;
			} else {
				nfo->_sample_limit[i] = -1;
			}
		}
		for (i = 0; i < nfo->_rates.size(); i++) {
			if (max_tp < nfo->_cur_tp[i]) {
				index_max_tp = i;
				max_tp = nfo->_cur_tp[i];
			}
			if (max_prob < nfo->_probability[i]) {
				index_max_prob = i;
				max_prob = nfo->_probability[i];
			}
		}
		max_tp = 0;
		for (i = 0; i < nfo->_rates.size(); i++) {
			if (i == index_max_tp) {
				continue;
			}
			if (max_tp < nfo->_cur_tp[i]) {
				index_max_tp2 = i;
				max_tp = nfo->_cur_tp[i];
			}
		}
		nfo->max_tp_rate = index_max_tp;
		nfo->max_tp_rate2 = index_max_tp2;
		nfo->max_prob_rate = index_max_prob;
	}
	_timer.schedule_after_msec(_period);
}

int Minstrel::initialize(ErrorHandler *)
{
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

int Minstrel::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_offset = 0;
	_ewma_level = 75;
	_lookaround_rate = 20;
	_period = 1000;
	_mrr = false;
	_active = true;
	_debug = false;
	if (cp_va_kparse(conf, this, errh,
				"RT", cpkM, cpElement, &_rtable,
				"OFFSET", 0, cpUnsigned, &_offset,
				"PERIOD", 0, cpUnsigned, &_period,
				"LOOKAROUND_RATE", 0, cpUnsigned, &_lookaround_rate,
				"EWMA_LEVEL", 0, cpUnsigned, &_ewma_level,
				"ACTIVE", 0, cpBool, &_active,
				"DEBUG", 0, cpBool, &_debug,
			 cpEnd) < 0)
		return -1;

	return 0;
}

void Minstrel::process_feedback(Packet *p_in) {
	if (!p_in) {
		return;
	}
	uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
	uint8_t *src_ptr = (uint8_t *) p_in->data() + _offset + 6;
	EtherAddress dst = EtherAddress(dst_ptr);
	EtherAddress src = EtherAddress(src_ptr);
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
	int success = !(ceh->flags & WIFI_EXTRA_TX_FAIL);
	/* don't record info for bcast packets */
	if (dst == EtherAddress::make_broadcast()) {
		return;
	}
	/* rate wasn't set */
	if (ceh->rate == 0) {
		return;
	}
	DstInfo *nfo = _neighbors.findp(dst);
	if (!nfo) {
		if (_debug) {
			click_chatter("%{element} :: %s :: no info for %s",
					this, 
					__func__,
					dst.unparse().c_str());
		}
		return;
	}
	nfo->add_result(ceh->rate, ceh->retries + 1, success);
	return;
}

uint32_t Minstrel::get_retry_count(uint32_t, uint32_t)
{
	return 4;
}

void Minstrel::assign_rate(Packet *p_in)
{

	if (!p_in) {
		return;
	}

	uint8_t *dst_ptr = (uint8_t *) p_in->data() + _offset;
	EtherAddress dst = EtherAddress(dst_ptr);
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);

	memset((void*)ceh, 0, sizeof(struct click_wifi_extra));

	if (dst.is_group() || !dst) {
		Vector<int> rates = _rtable->lookup(EtherAddress::make_broadcast());
		if (rates.size()) {
			ceh->rate = rates[0];
		} else {
			ceh->rate = 2;
		}
		return;
	}

	DstInfo *nfo = _neighbors.findp(dst);
	if (!nfo || !nfo->_rates.size()) {
		if (_debug) {
			click_chatter("%{element} :: %s :: adding %s",
					this, 
					__func__,
					dst.unparse().c_str());
		}
		_neighbors.insert(dst, DstInfo(dst, _rtable->lookup(dst)));
		nfo = _neighbors.findp(dst);
	}

	int ndx, sample_ndx = 0;
	bool sample = false;
	int delta;

	ndx = nfo->max_tp_rate;
	nfo->packet_count++;
	delta = (nfo->packet_count * _lookaround_rate / 100) - (nfo->sample_count);

	/* delta > 0: sampling required */
	if (delta > 0) {
		if (nfo->packet_count >= 10000) {
			nfo->sample_count = 0;
			nfo->packet_count = 0;
		} 
		sample_ndx = click_random(1, nfo->_rates.size() - 1);
		sample = false;
		if (nfo->_sample_limit[sample_ndx] != 0) {
			sample = true;
			ndx = sample_ndx;
			nfo->sample_count++;
			if (nfo->_sample_limit[sample_ndx] > 0) {
				nfo->_sample_limit[sample_ndx]--;
			}
		} 
	}
	/* If the sampling rate already has a probability 
	 * of >95%, we shouldn't be attempting to use it, 
	 * as this only wastes precious airtime */
	if (sample && (nfo->_probability[ndx] > 17100)) {
		ndx = nfo->max_tp_rate;
		sample = false;
	}

	ceh->magic = WIFI_EXTRA_MAGIC;

	if (!_mrr) {
		ceh->rate = nfo->_rates[nfo->max_tp_rate];
		ceh->max_tries = WIFI_MAX_RETRIES + 1;
		return;
	}

	if (sample) {
		if (nfo->_rates[ndx] < nfo->_rates[nfo->max_tp_rate]) {
			ceh->rate = nfo->_rates[nfo->max_tp_rate];
			ceh->rate1 = nfo->_rates[ndx];
		} else {
			ceh->rate = nfo->_rates[ndx];
			ceh->rate1 = nfo->_rates[nfo->max_tp_rate];
		}
	} else {
		ceh->rate = nfo->_rates[nfo->max_tp_rate];
		ceh->rate1 = nfo->_rates[nfo->max_tp_rate2];
	}

	ceh->max_tries = get_retry_count(p_in->length(), ceh->rate);
	ceh->max_tries1 = get_retry_count(p_in->length(), ceh->rate1);

	ceh->retries = ceh->max_tries - 1;

	ceh->rate2 = nfo->_rates[nfo->max_prob_rate];
	ceh->max_tries2 = get_retry_count(p_in->length(), ceh->rate2);

	ceh->rate3 = nfo->_rates[0];
	ceh->max_tries3 = get_retry_count(p_in->length(), ceh->rate3);

	return;

}

Packet* Minstrel::pull(int port) {
	Packet *p = input(port).pull();
	if (p && _active) {
		assign_rate(p);
	}
	return p;
}

void Minstrel::push(int port, Packet *p_in)
{
	if (!p_in) {
		return;
	}
	if (port != 0) {
		process_feedback(p_in);
	} else {
		assign_rate(p_in);
	}
	checked_output_push(port, p_in);
}

String Minstrel::print_rates()
{
	StringAccum sa;
	int i, tp, prob, eprob;
	char buffer[4096];
	for (NeighborIter iter = _neighbors.begin(); iter.live(); iter++) {
		DstInfo nfo = iter.value();
		sa << nfo._eth << "\n";
		sa << "rate     throughput  ewma prob   this prob  this success(attempts)   success    attempts\n";
		for (i = 0; i < nfo._rates.size(); i++) {
			tp = nfo._cur_tp[i] / ((18000 << 10) / 96);
			prob = nfo._cur_prob[i] / 18;
			eprob = nfo._probability[i] / 18;
			sprintf(buffer, "%2d%s    %2u.%1u    %3u.%1u    %3u.%1u    %3u(%3u)    %8llu    %8llu\n",
					nfo._rates[i] / 2, nfo._rates[i] & 1 ? ".5" : "  ",
					tp / 10, tp % 10,
					eprob / 10, eprob % 10,
					prob / 10, prob % 10,
					nfo._last_success[i],
					nfo._last_attempts[i],
					(unsigned long long) nfo._hist_success[i],
					(unsigned long long) nfo._hist_attempts[i]);
			if (i == nfo.max_tp_rate)
				sa << 'T';
			else if (i == nfo.max_tp_rate2)
				sa << 't';
			else
				sa << ' ';
			if (i == nfo.max_prob_rate)
				sa << 'P';
			else
				sa << ' ';
			sa << buffer;
		}
		sa << "\n"
		   << "Total packet count: ideal " 
		   << (nfo.packet_count - nfo.sample_count) 
		   << " lookaround "
		   << nfo.sample_count
		   << " total "
		   << nfo.packet_count
		   << "\n\n";
	}
	return sa.take_string();
}

enum {
	H_RATES, H_DEBUG
};

String Minstrel::read_handler(Element *e, void *thunk) {
	Minstrel *c = (Minstrel *) e;
	switch ((intptr_t) (thunk)) {
	case H_DEBUG:
		return String(c->_debug) + "\n";
	case H_RATES:
		return c->print_rates();
	default:
		return "<error>\n";
	}
}

int Minstrel::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	Minstrel *d = (Minstrel *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		d->_debug = debug;
		break;
	}
	}
	return 0;
}

void Minstrel::add_handlers() {
	add_read_handler("rates", read_handler, H_RATES);
	add_read_handler("debug", read_handler, H_DEBUG);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Minstrel)

