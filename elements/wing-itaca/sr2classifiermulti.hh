#ifndef CLICK_SR2CLASSIFIERMULTI_HH
#define CLICK_SR2CLASSIFIERMULTI_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include "availableinterfaces.hh"
#include "sr2pathmulti.hh"
#include "sr2nodemulti.hh"
CLICK_DECLS

/*
 * =c
 * SR2QueryResponder(ETHERTYPE, IP, ETH, LinkTable element, ARPTable element)
 * =s Wifi, Wireless Routing
 * Responds to queries destined for this node.
 */

class SR2ClassifierMulti : public Element {
 public:
  
  SR2ClassifierMulti();
  ~SR2ClassifierMulti();
  
  const char *class_name() const		{ return "SR2ClassifierMulti"; }
  const char *port_count() const		{ return "1/-"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/#"; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);

  /* handler stuff */
//  void add_handlers();

  void push(int, Packet *);
  Packet * rewrite_src(Packet *);
  Packet * rewrite_dst(Packet *);
  int get_output_incoming(EtherAddress);
  Packet* checkdst(Packet*);
  int checksrc(EtherAddress);

 private:

  IPAddress _ip;    // My IP address.
  uint16_t _et;     // This protocol's ethertype

  HashMap<EtherAddress,AvailableInterfaces::LocalIfInfo> _ifaces;

  class SR2LinkTableMulti *_link_table;
  class ARPTableMulti *_arp_table;
  class AvailableInterfaces *_if_table;

  bool _debug;
  bool _isdest;



//  static int write_handler(const String &, Element *, void *, ErrorHandler *);
//  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
