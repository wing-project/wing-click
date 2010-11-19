#ifndef CLICK_SR2CHANNELRESPONDERMULTI_HH
#define CLICK_SR2CHANNELRESPONDERMULTI_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include "sr2channelselectormulti.hh"
CLICK_DECLS

/*
 * =c
 * SR2ChannelResponder(ETHTYPE, IP, ETH, PERIOD, LinkTable element, 
 * ARPTable element, GatewaySelector element)
 * =s Wifi, Wireless Routing
 * Responds to queries destined for this node.
 */

class SR2ChannelResponderMulti : public Element {
 public:
  
  SR2ChannelResponderMulti();
  ~SR2ChannelResponderMulti();
  
  const char *class_name() const		{ return "SR2ChannelResponderMulti"; }
  const char *port_count() const		{ return "0/1"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/#"; }

  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void run_timer(Timer *);
  void start_sniff();
  void send_scandata();
  void add_handlers();

 private:

  IPAddress _ip;         // My IP address.
  uint32_t _et;          // This protocol's ethertype
  unsigned int _period;  // msecs
  unsigned int _chan_period;  // msecs
  EtherAddress _eth;
  int _original_channel;
  int *_channel;
  int _total_channels;
	int _version;
  int _channel_index;
  bool _sniffing;
  bool _debug;
  
  String _chstr;

  class ARPTableMulti *_arp_table;
  class SR2ChannelSelectorMulti *_ch_sel;
  class SR2LinkTableMulti *_link_table;
  class AvailableInterfaces *_if_table;
  class WirelessInfo *_winfo;
  class SR2CounterMulti *_pkcounter;

  SR2ChannelInfo _ctable;
  Timer _timer;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
