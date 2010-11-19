/*
 * availableSensors.{cc,hh} -- List of sensors in a cluster
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
#include "availablesensors.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

AvailableSensors::AvailableSensors() 
  :  _master_key(0),
     _msg(1)
{
  _sensors = new SensorsTable(0);
}

AvailableSensors::~AvailableSensors()
{
}

int
AvailableSensors::configure(Vector<String> &conf, ErrorHandler *errh)
{	
	int before = errh->nerrors();
	_as = new AvailableSensors *[conf.size()] ;
	_as_size = conf.size();
	for (int i = 0; i < conf.size(); i++) {
		Element *e = cp_element(conf[i], router(), errh);
		_as[i] = (AvailableSensors *)e;
	}
	return (errh->nerrors() == before ? 0 : -1);
}

enum {H_INSERT, H_CLEAR, H_SENSORS};

String
AvailableSensors::read_handler(Element *e, void *thunk)
{
  AvailableSensors *td = (AvailableSensors *)e;
  switch ((uintptr_t) thunk) {
  case H_SENSORS: {
    StringAccum sa;
    SensorsItr itr = td->_sensors->begin();
    while(itr != td->_sensors->end()){
      sa << "Sensor id: " << itr << "\n";
      itr++;
    } 
    return sa.take_string();
  }
  default:
    return String();
  }
}

int 
AvailableSensors::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  AvailableSensors *f = (AvailableSensors *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_INSERT: {  
    unsigned sensor;
    if (!cp_integer(s, &sensor)) 
      return errh->error("sensor parameter must be an unsigned");
    f->add_sensor(sensor);
    break;
  }
  case H_CLEAR:
    f->_sensors->clear();
    break;
  }
  return 0;
}

void
AvailableSensors::add_handlers()
{
  add_read_handler("insert", read_handler, (void *) H_INSERT);
  add_read_handler("clear", read_handler, (void *) H_CLEAR);
  add_write_handler("sensors", write_handler, (void *) H_SENSORS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AvailableSensors)

