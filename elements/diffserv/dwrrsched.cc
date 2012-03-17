/*
 * dwrr.{cc,hh} -- deficit round-robin scheduler
 * Robert Morris, Eddie Kohler, Roberto Riggio
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
 * Copyright (c) 2008 CREATE-NET
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
#include "dwrrsched.hh"
#include <click/confparse.hh>
#include <clicknet/ether.h>
#include <elements/wing/arptablemulti.hh>
#include <elements/wing/linktablemulti.hh>
CLICK_DECLS

DWRRSched::DWRRSched()
    : _head(0), _deficit(0), _signals(0), _notifier(Notifier::SEARCH_CONTINUE_WAKE), _next(0), _scaling(1500)
{
}

DWRRSched::~DWRRSched()
{
}

void *
DWRRSched::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return &_notifier;
    else
	return Element::cast(n);
}

int
DWRRSched::configure(Vector<String> &conf, ErrorHandler *errh)
{	
	_link_table=0;
	_arp_table=0;

	int before = errh->nerrors();

	if (conf.size() < ninputs())
		 errh->error("need %d arguments, one per input port", ninputs());

	_quantum = new int[ninputs()];
	Vector<String> conf_vec;

	for (int i = 0; i < conf.size(); i++) {
		
		if (i < ninputs()) {
			int v;
			if (!cp_integer(conf[i], &v)) {
				errh->error("argument %d should be a weight (integer)", i);
			} else if (v <= 0) {
				errh->error("argument %d (weight) must be > 0", i);
			} else {
				_quantum[i] = v;
			}    
		} else {
			conf_vec.push_back(conf[i]);
		}

	}
	
	cp_va_kparse(conf_vec, this, errh,
			"LT", 0, cpElement, &_link_table, 
			"ARP", 0, cpElement, &_arp_table,
			cpEnd);

	if (_link_table && _link_table->cast("LinkTableMulti") == 0) {
		return errh->error("LT element is not a LinkTableMulti");
	}

	if (_arp_table && _arp_table->cast("ARPTableMulti") == 0) {
		return errh->error("ARP element is not a ARPTableMulti");
	}

	_notifier.initialize(Notifier::EMPTY_NOTIFIER, router());

	return (errh->nerrors() == before ? 0 : -1);
}

int
DWRRSched::initialize(ErrorHandler *errh)
{
    _head = new Packet *[ninputs()];
    _deficit = new unsigned[ninputs()];
    _signals = new NotifierSignal[ninputs()];
    if (!_head || !_deficit || !_signals)
	return errh->error("out of memory!");

    int mtu = 1500;

    for (int i = 0; i < ninputs(); i++) {
	_head[i] = 0;
	_deficit[i] = 0;
	_quantum[i] = _quantum[i] * mtu;
	_signals[i] = Notifier::upstream_empty_signal(this, i, &_notifier);
    }

    _next = 0;
    return 0;
}

void
DWRRSched::cleanup(CleanupStage)
{
    if (_head)
	for (int j = 0; j < ninputs(); j++)
	    if (_head[j])
		_head[j]->kill();
    delete[] _head;
    delete[] _deficit;
    delete[] _signals;
}

uint32_t 
DWRRSched::compute_deficit(const Packet* p)
{
  if (!p)
    return 0;
  if (_link_table) {
    click_ether *e = (click_ether *) p->data();
    EtherAddress *dhost = new EtherAddress(e->ether_dhost);
    IPAddress ip = _arp_table->reverse_lookup(*dhost);
    uint32_t metric = _link_table->get_host_metric_from_me(ip);
    if (!metric)
      return 0;
    return (p->length() * metric) / _scaling;
  } else {
    return p->length();
  }
}

Packet *
DWRRSched::pull(int)
{
    int n = ninputs();
    bool signals_on = false;

    // Look at each input once, starting at the *same*
    // one we left off on last time.
    for (int j = _next; j < n; j++) {
	Packet *p;
	if (_head[_next]) {
	    p = _head[_next];
	    _head[_next] = 0;
	    signals_on = true;
	} else if (_signals[_next]) {
	    p = input(_next).pull();
	    signals_on = true;
	} else
	    p = 0;

	if (p == 0)
	    _deficit[_next] = 0;
	else if (compute_deficit(p) <= _deficit[_next]) {
	    _deficit[_next] -= compute_deficit(p);
	    _notifier.set_active(true);
	    return p;
	} else 
	    _head[_next] = p;

	_next++;
	if (_next >= n)
	    _next = 0;

	_deficit[_next] += _quantum[_next];
    }

    _notifier.set_active(signals_on);
    return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(DWRRSched)
