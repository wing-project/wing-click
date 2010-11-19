#ifndef CLICK_AVAILABLESENSORS_HH
#define CLICK_AVAILABLESENSORS_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include "sapacket.hh"
CLICK_DECLS

class AvailableSensors : public Element { public:

  AvailableSensors();
  ~AvailableSensors();

  const char *class_name() const		{ return "AvailableSensors"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);

  void add_handlers();

  bool add_sensor(uint8_t sensor) { 
    return _sensors->set(sensor, sa_sensor_key(_master_key, sensor)); 
  }

  uint32_t get_key(uint8_t sensor) { 
    return _sensors->get(sensor);
  }

  uint8_t msg () { return _msg; }

  typedef HashTable<uint8_t, uint32_t> SensorsTable;
  typedef SensorsTable::const_iterator SensorsItr;

  void fetch_upstream_sensors(SensorsTable *st, bool recursive) {
    SensorsItr itr = _sensors->begin();
    while(itr != _sensors->end()) {
      st->set(itr.key(),itr.value());
      itr++;
    }
    if (recursive) {
      for (unsigned i = 0; i < _as_size; i++) {
        _as[i]->fetch_upstream_sensors(st, recursive);
      }
    }
  }

  Vector<uint8_t>* diff(Vector<uint8_t> *sensors, bool recursive) {
    Vector<uint8_t> *diff = new Vector<uint8_t>();
    SensorsTable* st = new SensorsTable(0);
    fetch_upstream_sensors(st, recursive);
    SensorsItr itr = st->begin();
    while(itr != st->end()) {
      bool found = false;
      for (int j = 0; j < sensors->size(); j++) {
        if (sensors->at(j) == itr.key()) {
          found = true;
          break;
        }
      }
      if (!found) {
        diff->push_front(itr.key());
      }
      itr++;
    }
    return diff;
  }

  uint32_t get_keystream(uint8_t sensor) { 
    SensorsTable* st = new SensorsTable(0);
    fetch_upstream_sensors(st, true);
    return sa_keystream(st->get(sensor), _msg);
  }

  uint32_t get_keystream_p(Vector<uint8_t> *sensors) { 
    uint32_t keystream = 0;
    SensorsTable* st = new SensorsTable(0);
    fetch_upstream_sensors(st, true);
    for (int i = 0; i < sensors->size(); i++) {
      keystream = sa_add_keystreams(keystream, sa_keystream(st->get(sensors->at(i)), _msg));
    }
    return keystream;
  }

  uint32_t get_keystream_n(Vector<uint8_t> *sensors) { 
    uint32_t keystream = 0;
    SensorsTable* st = new SensorsTable(0);
    fetch_upstream_sensors(st, true);
    SensorsItr itr = st->begin();
    while(itr != st->end()) {
      bool found = false;
      for (int j = 0; j < sensors->size(); j++) {
        if (sensors->at(j) == itr.key()) {
          found = true;
          break;
        }
      }
      if (!found) {
        keystream = sa_add_keystreams(keystream, sa_keystream(itr.value(), _msg));
      }
      itr++;
    }
    return keystream;
  }

  uint8_t nb_sensors () { 
    SensorsTable* st = new SensorsTable(0);
    fetch_upstream_sensors(st, true);
    return st->size(); 
  }

  SensorsTable* _sensors;

 private:

  uint32_t _master_key;
  uint8_t _msg;
  class AvailableSensors **_as;
  uint32_t _as_size;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
