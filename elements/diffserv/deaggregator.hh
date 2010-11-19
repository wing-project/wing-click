#ifndef CLICK_DEAGGREGATOR_HH
#define CLICK_DEAGGREGATOR_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * DeAggregator()
 * =s ethernet
 * splits incoming packets
 * =io
 * one output, one input
 * =d
 * Expects aggregated packets as input. Splits incoming packets.
 */

class DeAggregator : public Element { public:
  
  DeAggregator();
  ~DeAggregator();
  
  const char *class_name() const	{ return "DeAggregator"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }
  
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *);

  void push(int, Packet *);

private:

  Packet *_p;
  uint16_t _et;     // This protocol's ethertype

};

CLICK_ENDDECLS
#endif
