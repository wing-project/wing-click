/*
 * arptablemulti.{cc,hh} -- ARP resolver element
 * Eddie Kohler, Stefano Testi, Roberto Riggio
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2005 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
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
#include "arptablemulti.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

ARPTableMulti::ARPTableMulti() :
	_entry_capacity(0), _packet_capacity(2048),
	_expire_jiffies(300 * CLICK_HZ), _expire_timer(this) {
	_entry_count = _packet_count = _drops = 0;
}

ARPTableMulti::~ARPTableMulti() {
}

int ARPTableMulti::configure(Vector<String> &conf, ErrorHandler *errh) {
	Timestamp timeout(300);
	if (cp_va_kparse(conf, this, errh, "CAPACITY", 0, cpUnsigned,
			&_packet_capacity, "ENTRY_CAPACITY", 0, cpUnsigned,
			&_entry_capacity, "TIMEOUT", 0, cpTimestamp, &timeout, cpEnd) < 0)
		return -1;
	set_timeout(timeout);
	if (_expire_jiffies) {
		_expire_timer.initialize(this);
		_expire_timer.schedule_after_sec(_expire_jiffies / CLICK_HZ);
	}
	return 0;
}

void ARPTableMulti::cleanup(CleanupStage) {
	clear();
}

void ARPTableMulti::clear() {
	// Walk the arp cache table and free any stored packets and arp entries.
	for (ARPTable::iterator it = _arp_table.begin(); it;) {
		ARPEntry *ae = _arp_table.erase(it);
		while (Packet *p = ae->_head) {
			ae->_head = p->next();
			p->kill();
			++_drops;
		}
		_alloc.deallocate(ae);
	}
	_entry_count = _packet_count = 0;
	_age.__clear();
}

void ARPTableMulti::take_state(Element *e, ErrorHandler *errh) {
	ARPTableMulti *arpt = (ARPTableMulti *) e->cast("ARPTableMulti");
	if (!arpt)
		return;
	if (_arp_table.size() > 0) {
		errh->error("late take_state");
		return;
	}
	_arp_table.swap(arpt->_arp_table);
	_age.swap(arpt->_age);
	_entry_count = arpt->_entry_count;
	_packet_count = arpt->_packet_count;
	_drops = arpt->_drops;
	_alloc.swap(arpt->_alloc);
	arpt->_entry_count = 0;
	arpt->_packet_count = 0;
}

void ARPTableMulti::slim() {
	click_jiffies_t now = click_jiffies();
	ARPEntry *ae;
	// Delete old entries.
	while ((ae = _age.front()) && (ae->expired(now, _expire_jiffies)
			|| (_entry_capacity && _entry_count > _entry_capacity))) {
		_arp_table.erase(ae->_node);
		_age.pop_front();
		while (Packet *p = ae->_head) {
			ae->_head = p->next();
			p->kill();
			--_packet_count;
			++_drops;
		}
		_alloc.deallocate(ae);
		--_entry_count;
	}
	// Mark entries for polling, and delete packets to make space.
	while (_packet_capacity && _packet_count > _packet_capacity) {
		while (ae->_head && _packet_count > _packet_capacity) {
			Packet *p = ae->_head;
			if (!(ae->_head = p->next()))
				ae->_tail = 0;
			p->kill();
			--_packet_count;
			++_drops;
		}
		ae = ae->_age_link.next();
	}
}

void ARPTableMulti::run_timer(Timer *timer) {
	// Expire any old entries, and make sure there's room for at least one packet.
	_lock.acquire_write();
	slim();
	_lock.release_write();
	if (_expire_jiffies)
		timer->schedule_after_sec(_expire_jiffies / CLICK_HZ + 1);
}

ARPTableMulti::ARPEntry *
ARPTableMulti::ensure(NodeAddress node) {
	_lock.acquire_write();
	ARPTable::iterator it = _arp_table.find(node);
	if (!it) {
		void *x = _alloc.allocate();
		if (!x) {
			_lock.release_write();
			return 0;
		}
		++_entry_count;
		if (_entry_capacity && _entry_count > _entry_capacity)
			slim();
		ARPEntry *ae = new (x) ARPEntry(node);
		ae->_live_jiffies = click_jiffies();
		ae->_poll_jiffies = ae->_live_jiffies - CLICK_HZ;
		_arp_table.set(it, ae);
		_age.push_back(ae);
	}
	return it.get();
}

int ARPTableMulti::insert(NodeAddress node, const EtherAddress &eth,
		Packet **head) {
	ARPEntry *ae = ensure(node);
	if (!ae)
		return -ENOMEM;
	ae->_eth = eth;
	ae->_unicast = !eth.is_broadcast();
	ae->_live_jiffies = click_jiffies();
	ae->_poll_jiffies = ae->_live_jiffies - CLICK_HZ;
	if (ae->_age_link.next()) {
		_age.erase(ae);
		_age.push_back(ae);
	}
	if (head) {
		*head = ae->_head;
		ae->_head = ae->_tail = 0;
		for (Packet *p = *head; p; p = p->next())
			--_packet_count;
	}
	_arp_table.balance();
	_lock.release_write();
	return 0;
}

int ARPTableMulti::append_query(NodeAddress node, Packet *p) {
	ARPEntry *ae = ensure(node);
	if (!ae)
		return -ENOMEM;
	click_jiffies_t now = click_jiffies();
	if (ae->unicast(now, _expire_jiffies)) {
		_lock.release_write();
		return -EAGAIN;
	}
	++_packet_count;
	if (_packet_capacity && _packet_count > _packet_capacity)
		slim();
	if (ae->_tail)
		ae->_tail->set_next(p);
	else
		ae->_head = p;
	ae->_tail = p;
	p->set_next(0);
	int r;
	if (!click_jiffies_less(now, ae->_poll_jiffies + CLICK_HZ / 10)) {
		ae->_poll_jiffies = now;
		r = 1;
	} else {
		r = 0;
	}
	_arp_table.balance();
	_lock.release_write();
	return r;
}

NodeAddress ARPTableMulti::reverse_lookup(const EtherAddress &eth) {
	_lock.acquire_read();
	NodeAddress node;
	for (ARPTable::iterator it = _arp_table.begin(); it; ++it) {
		if (it->_eth == eth) {
			node = it->_node;
			break;
		}
	}
	_lock.release_read();
	return node;
}

String ARPTableMulti::print_arp_table() {
	StringAccum sa;
	click_jiffies_t now = click_jiffies();
	for (ARPTable::const_iterator it = _arp_table.begin(); it; ++it) {
		int ok = it->unicast(now, _expire_jiffies);
		sa << it->_node.unparse() << ' ' << ok << ' ' << it->_eth << ' '
				<< Timestamp::make_jiffies(now - it->_live_jiffies) << "\n";
	}
	return sa.take_string();
}

enum {
	H_TABLE, H_DROPS, H_INSERT, H_DELETE, H_CLEAR
};

String ARPTableMulti::read_handler(Element *e, void *thunk) {
	ARPTableMulti *c = (ARPTableMulti *) e;
	switch ((intptr_t) (thunk)) {
	case H_TABLE:
		return c->print_arp_table();
	case H_DROPS:
		return String(c->_drops) + "\n";
	default:
		return "<error>\n";
	}
}

int ARPTableMulti::write_handler(const String &in_s, Element *e, void *vparam,
		ErrorHandler *errh) {
	ARPTableMulti *d = (ARPTableMulti *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_INSERT: {
		IPAddress ip;
		uint16_t iface;
		EtherAddress eth;
		if (cp_va_space_kparse(in_s, d, errh, "IP", cpkP + cpkM, cpIPAddress,
				&ip, "IFACE", cpkP + cpkM, cpInteger, &iface, "ETH", cpkP
						+ cpkM, cpEtherAddress, &eth, cpEnd) < 0)
			return -1;
		d->insert(NodeAddress(ip, iface), eth);
		break;
	}
	case H_DELETE: {
		IPAddress ip;
		uint16_t iface;
		if (cp_va_space_kparse(in_s, d, errh, "IP", cpkP + cpkM, cpIPAddress,
				&ip, "IFACE", cpkP + cpkM, cpInteger, &iface, cpEnd) < 0)
			return -1;
		d->insert(NodeAddress(ip, iface), EtherAddress::make_broadcast());
		break;
	}
	case H_CLEAR: {
		d->clear();
		break;
	}
	}
	return 0;
}

void ARPTableMulti::add_handlers() {
	add_read_handler("table", read_handler, H_TABLE);
	add_read_handler("drops", read_handler, H_DROPS);
	add_write_handler("insert", write_handler, H_INSERT);
	add_write_handler("delete", write_handler, H_DELETE);
	add_write_handler("clear", write_handler, H_CLEAR);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ARPTableMulti)
ELEMENT_MT_SAFE(ARPTableMulti)
