/*
 * AvailableSensorsIndex.{cc,hh} -- List of sensors in a cluster
 * Roberto Riggio
 *
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
#include "availablesensorsindex.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

AvailableSensorsIndex::AvailableSensorsIndex() 
{
  _as_table = new AvailableSensorsTable(0);
}

AvailableSensorsIndex::~AvailableSensorsIndex()
{
}

int
AvailableSensorsIndex::configure(Vector<String> &conf, ErrorHandler *errh)
{	
	int before = errh->nerrors();
	for (int i = 0; i < conf.size(); i++) {
		AvailableSensors *as = (AvailableSensors *)cp_element(conf[i], router(), errh);
		_as_table->set(as->msg(), as);
	}
	return (errh->nerrors() == before ? 0 : -1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AvailableSensorsIndex)

