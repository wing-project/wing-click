/*
 * sensor.{cc,hh} -- sensor emulator
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
#include "sensor.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/vector.hh>
#include "sapacket.hh"
#include "availablesensors.hh"
CLICK_DECLS

Sensor::Sensor()
  :  _sensor(0),
     _value(10),
     _max_jitter(0)
{
}

Sensor::~Sensor()
{
}

int
Sensor::configure (Vector<String> &conf, ErrorHandler *errh)
{
  _debug = false;
  uint32_t sampling_prob = 0xFFFFFFFFU;
  int ret = cp_va_kparse(conf, this, errh,
		     "ID", 0, cpUnsigned, &_sensor,
		     "VALUE", 0, cpUnsigned, &_value,
		     "MAX_JITTER", 0, cpUnsigned, &_max_jitter,
		     "SENSORS", 0, cpElement, &_as,
		     "SAMPLE", 0, cpUnsignedReal2, SAMPLING_SHIFT, &sampling_prob,
		     "DEBUG", 0, cpBool, &_debug,
		     cpEnd);

  if (sampling_prob > (1 << SAMPLING_SHIFT))
    return errh->error("sampling probability must be between 0 and 1");
  if (_sensor <= 0)
    return errh->error("ID must be a positive integer");
  if (!_as) 
    return errh->error("SENSORS not specified");
  if (_as->cast("AvailableSensors") == 0) 
    return errh->error("SENSORS element is not a AvailableSensors");

  _sampling_prob = sampling_prob;

  return ret;
}

int
Sensor::initialize (ErrorHandler *errh)
{
  if (_as->get_key(_sensor) != 0) {
    return errh->error("Duplicate use of sensor id %u", _sensor);
  }
  _as->add_sensor(_sensor);
  _keystream = _as->get_keystream(_sensor);
  if (!(_signals = new NotifierSignal[ninputs()]))
    return errh->error("out of memory!");
  for (int i = 0; i < ninputs(); i++)
    _signals[i] = Notifier::upstream_empty_signal(this, i, 0);
  return 0;
}

Packet *
Sensor::pull(int)
{

  uint32_t cipher_avg = 0;
  uint32_t cipher_var = 0;

  Vector<uint8_t> *sensors = new Vector<uint8_t>();

  if ((click_random() & SAMPLING_MASK) < _sampling_prob) {

    unsigned jitter = click_random(0, 2 * _max_jitter);
    uint32_t avg = _value + jitter - _max_jitter;
    uint32_t var = avg * avg;
  
    if (_debug) {
  	click_chatter("%{element} :: %s :: broadcasting (%u, %u, %u)", 
			this, 
			__func__, 
			_sensor, 
			_as->msg(), 
			avg);
    } 

    sensors->push_front(_sensor);
    cipher_avg = sa_enc(avg, _keystream);
    cipher_var = sa_enc(var, _keystream);

  }

  int n = ninputs();
  for (int i = 0; i < n; i++) {
    Packet *p = (_signals[i] ? input(i).pull() : 0);
    if (!p) {
      continue;
    }
    struct sapacket *pk = (struct sapacket *) p->data();
    cipher_avg = sa_add_ciphers(cipher_avg, pk->avg());
    cipher_var = sa_add_ciphers(cipher_var, pk->var());
    if (pk->type() == SA_PT_PROBE) {
      sensors->push_front(pk->sensors());
    } else if (pk->type() == SA_PT_PACK_P) {
      for (int i = 0; i < pk->sensors(); i++) {
        sensors->push_front(pk->get_sensor(i));
      }
    } else {
      click_chatter("%{element} :: %s :: unknown packet type", this, __func__);
    }
    p->kill();
  }

  if (sensors->size() == 0) {
    return 0;
  }

  WritablePacket *p = Packet::make(sizeof(sapacket));
  struct sapacket *sp = (struct sapacket *) p->data();
  memset(sp, '\0', sizeof(sapacket));
  sp->set_version(_version);
  sp->set_msg(_as->msg());
  sp->set_avg(cipher_avg);
  sp->set_var(cipher_var);

  if (sensors->size() == 1) {
    sp->set_type(SA_PT_PROBE);
    sp->set_sensors(_sensor);
  } else {
    sp->set_type(SA_PT_PACK_P);
    sp->set_sensors(sensors->size());
    for (int i = 0; i < sensors->size(); i++) {
      sp->set_sensor(i, sensors->at(i));
    }
  }

  return p;

}

CLICK_ENDDECLS
EXPORT_ELEMENT(Sensor)
