#ifndef CLICK_RADIOTAPENCAPHT_HH
#define CLICK_RADIOTAPENCAPHT_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * RadiotapEncapHT()
 *
 * =s Wifi
 *
 * Pushes the click_wifi_radiotap header on a packet based on information in Packet::anno()
 * =d
 * Copies the wifi_radiotap_header from Packet::anno() and pushes it onto the packet.
 * 
 * =a RadiotapDecap, SetTXRate
 */

class RadiotapEncapHT : public Element { public:

  RadiotapEncapHT();
  ~RadiotapEncapHT();

  const char *class_name() const	{ return "RadiotapEncapHT"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

private:

  bool _sgi;
  bool _ht40;
  

};

CLICK_ENDDECLS
#endif
