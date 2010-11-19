#ifndef CLICK_SINK_HH
#define CLICK_SINK_HH
#include <click/element.hh>
CLICK_DECLS

class Sink : public Element { public:
  
  Sink();
  ~Sink();
  
  const char *class_name() const	{ return "Sink"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &conf, ErrorHandler *errh);
  void push(int, Packet *);

 private:

  bool _debug;
  class AvailableSensors *_as;

};

CLICK_ENDDECLS
#endif
