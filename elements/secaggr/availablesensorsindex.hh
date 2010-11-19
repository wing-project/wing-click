#ifndef CLICK_AVAILABLESENSORSINDEX_HH
#define CLICK_AVAILABLESENSORSINDEX_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include "sapacket.hh"
#include "availablesensors.hh"
CLICK_DECLS

class AvailableSensorsIndex : public Element { public:

  AvailableSensorsIndex();
  ~AvailableSensorsIndex();

  const char *class_name() const		{ return "AvailableSensorsIndex"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *);

  AvailableSensors* get(uint8_t msg) { return _as_table->get(msg); }

 private:

  typedef HashTable<uint8_t, AvailableSensors*> AvailableSensorsTable;
  typedef AvailableSensorsTable::const_iterator ASTableItr;

  AvailableSensorsTable* _as_table;

};

CLICK_ENDDECLS
#endif
