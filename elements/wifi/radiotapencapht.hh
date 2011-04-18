#ifndef CLICK_RADIOTAPENCAPHT_HH
#define CLICK_RADIOTAPENCAPHT_HH
#include <click/element.hh>
#include <clicknet/ether.h>
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

  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
