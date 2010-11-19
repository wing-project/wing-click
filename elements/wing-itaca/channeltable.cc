/*
 * channeltable.{cc,hh} -- computes the per-second channel utilization time
 * Roberto Riggio
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
#include <click/error.hh>
#include "channeltable.hh"
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <clicknet/ether.h>
#include <elements/wifi/bitrate.hh>
#include "devinfo.hh"
#include "arptablemulti.hh"
#include "availablechannels.hh"
CLICK_DECLS

ChannelTable::ChannelTable() 
  : _timer(this), _next_channel(0), _period(5000), _parking(500)
{
}

ChannelTable::~ChannelTable() 
{
}

int ChannelTable::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	return 0;
}

void ChannelTable::run_timer(Timer *) {
	if (!_scanning) {
		// begins scanning
		_scanning = true;
		// save former channel
		_old_channel = _dev->channel();
		// fetch channels' list
		Vector<int> channels = ((AvailableChannels *)_dev->ctable())->lookup(_dev->eth());
		if (_next_channel > channels.size() - 1) {
			_next_channel = 0;
		} 
		// get next channel
		unsigned new_channel = channels[_next_channel];
		// broadcast interface down event
		// TODO: Implement XRLinkSwitcher and XRLinkRewriter elements
		// switch to new channel
		_dev->set_channel(new_channel);
		// reset status
		_last_updated = Timestamp::now();
		_interfering.clear();
		_utilization_counter = 0;
		// next channel
		_next_channel++;
		// schedule after channel parking time
		_timer.schedule_after_msec(_parking);
		return;
	}
	// stop scanning
	_scanning = false;
	// compute averages
	Timestamp window = Timestamp::now() - _last_updated;
	uint32_t utilization = (_utilization_counter * 100) / window.usecval();
	// update table
	ChnInfo *nfo = _channels.findp(_dev->channel());
	if (!nfo) {
		_channels.insert(_dev->channel(), ChnInfo());
		nfo = _channels.findp(_dev->channel());
	}
	nfo->_ewma_utilization.update(utilization);
	nfo->_utilization = utilization;
	nfo->_ewma_interfering.update(_interfering.size());
	nfo->_interfering = _interfering.size();
	// switch back to old channel
	_dev->set_channel(_old_channel);
	//re-schedule
	_timer.schedule_after_msec(_period);
}

int ChannelTable::configure(Vector<String> &conf, ErrorHandler *errh) {
	_scanning = false;
	_debug = false;
	if (cp_va_kparse(conf, this, errh, 
				"DEV", cpkM, cpElement, &_dev,
				"ARP", cpkM, cpElement, &_arp_table,
				"PARKING", 0, cpUnsigned, &_parking, 
				"PERIOD", 0, cpUnsigned, &_period, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	if (_dev->cast("DevInfo") == 0) {
		return errh->error("DEV element is not a DevInfo element");
	}
	if (_arp_table->cast("ARPTableMulti") == 0) {
		return errh->error("ARPTable element is not a ARPTableMulti");	
	}
	return 0;
}

Packet* ChannelTable::simple_action(Packet *p_in)
{
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);

	if (ceh->channel == 0)
		return p_in;

	if (_dev->channel() != ceh->channel) {
		click_chatter("%{element} :: %s :: %s's channel set to %u, packet says %u",
				this, 
				__func__, 
				_dev->eth().unparse().c_str(),
				_dev->channel(),
				ceh->channel);
		_dev->set_channel(ceh->channel)
	}

	if (! _scanning ) 
		return p_in;

	uint8_t *ptr = (uint8_t *) p_in->data() + 10;
	uint8_t *end = (uint8_t *) p_in->data() + p_in->length();

	if (ptr >= end) {
		ptr = (uint8_t *) p_in->data() + 4;
	}

	EtherAddress eth = EtherAddress(ptr);

	if (!_arp_table->reverse_lookup(eth)) {
		_interfering.insert(eth, 0);
		_utilization_counter += calc_usecs_wifi_packet(p_in->length(), ceh->rate, ceh->retries);
	} 

	return p_in;

}

String ChannelTable::print_stats() {
	StringAccum sa;
	sa << "Channel\tE-Util\tUtil\tE-Int\tInt\n";
	for (ChnIter iter = _channels.begin(); iter.live(); iter++) {
		int ewma_utilization = iter.value()._ewma_utilization.unscaled_average();
		int utilization = iter.value()._utilization;
		int ewma_interfering = iter.value()._ewma_interfering.unscaled_average();
		int interfering = iter.value()._interfering;
		sa << iter.key() << "\t" << ewma_utilization << "\t" << utilization << "\t" << ewma_interfering << "\t" << interfering << "\n";
	}
	return sa.take_string();
}

enum {
	H_DEBUG,
	H_STATS
};

String ChannelTable::read_handler(Element *e, void *thunk) {
	ChannelTable *c = (ChannelTable *) e;
	switch ((intptr_t) (thunk)) {
	case H_STATS:
		return c->print_stats();
	case H_DEBUG:
		return String(c->_debug) + "\n";
	default:
		return "<error>\n";
	}
}

int ChannelTable::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	ChannelTable *d = (ChannelTable *) e;
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

void ChannelTable::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_read_handler("stats", read_handler, H_STATS);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ChannelTable)
