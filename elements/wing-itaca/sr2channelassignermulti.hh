#ifndef CLICK_SR2CHANNELASSIGNERMULTI_HH
#define CLICK_SR2CHANNELASSIGNERMULTI_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/wifi/path.hh>
#include "sr2channelselectormulti.hh"
CLICK_DECLS

class NodeDelay {
  public:
    NodePair _nodep;
    int _delay;
		NodeDelay() : _nodep(), _delay(0){ }
		NodeDelay(NodePair nodep, int delay){
			_nodep = nodep;
			_delay = delay;
		}
	};

/*
 * =c
 * SR2ChannelAssignerMulti(IP, ETH, ETHTYPE, LinkTable element, ARPTable element,  
 *                    [PERIOD timeout], [GW is_gateway])
 * =s Wifi, Wireless Routing
 * Select a gateway to send a packet to based on TCP connection
 * state and metric to gateway.
 * =d
 * This element provides proactive gateway selection.  
 * Each gateway broadcasts an ad every PERIOD msec.  
 * Non-gateway nodes select the gateway with the best 
 * metric and forward ads.
 */

class SR2ChannelAssignerMulti : public Element {
 public:
  
  SR2ChannelAssignerMulti();
  ~SR2ChannelAssignerMulti();
  
  const char *class_name() const		{ return "SR2ChannelAssignerMulti"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);

  /* handler stuff */
//  void add_handlers();
//  String print_cas_stats();

  void push(int, Packet *);
  void run_timer(Timer *);
  
  void start_assignment();
    
  typedef HashMap<NodeAddress, SR2ScanInfo> NTable;
  typedef NTable::const_iterator NIter;

	class MCGLeaf {
	  public:
	    NodePair _nodep;
	    uint32_t _hop_count;
	    uint32_t _hop_from;
	    uint32_t _hop_to;
	    uint32_t _delay;
			uint32_t _traffic;
			HashMap<uint32_t,uint32_t> _channels;
	    HashMap<NodePair,uint32_t> _interfering;
	    MCGLeaf() : _nodep(), _hop_count(0), _delay(0), _traffic(0), _interfering() { }
			MCGLeaf(NodePair nodep, int hop_count, int delay, int traffic, HashMap<NodePair,int> interfering) {
	      _nodep = nodep;
	      _hop_count = hop_count;
	      _delay = delay;
				_traffic = traffic;
				for(HashMap<NodePair,int>::const_iterator iter = interfering.begin(); iter.live(); iter++){
					_interfering.insert(iter.key(),iter.value());
				}
	    }
			void update_channel(int channel,int traffic) {
				uint32_t * chan = _channels.findp(channel);
				if (!chan){
					_channels.insert(channel,traffic);
				} else {
					*chan = *chan + traffic;
				}
			}
			uint32_t best_channel() {
				uint32_t channel = 0;
				uint32_t best = ~0;
				for (HashMap<uint32_t,uint32_t>::const_iterator iter = _channels.begin(); iter.live(); iter++) {
					if (iter.value() < best) {
						channel = iter.key();
						best = iter.value();
					}
				}
				_channels.erase(channel);
				return channel;
			}
			void update_interfering(NodePair nodep,uint32_t metric) {
				NodePair pair_fw = NodePair(nodep._from,nodep._to);
				NodePair pair_rev = NodePair(nodep._to,nodep._from);
				uint32_t * nfo1 = _interfering.findp(pair_fw);
				uint32_t * nfo2 = _interfering.findp(pair_rev);
				if (!nfo1 && !nfo2 && !(_nodep == pair_fw) && !(_nodep == pair_rev)) {
					_interfering.insert(nodep,metric);
				}
			}
	  };

	typedef HashMap<NodePair, MCGLeaf> MCGraph;
	typedef MCGraph::const_iterator MCGIter;
	
	typedef HashMap<NodeAddress,int> CHTable;
	typedef CHTable::const_iterator CHIter;

	//int delay_sorter(const void *va, const void *vb, void *);
	Vector<NodeDelay> smallest_hop_count();
	Vector<NodeAddress> remove_neighbor_link(NodeAddress);
	void assign_channel(NodeAddress,int);
	Vector<NodeDelay> get_link_from_node(NodeAddress);
		
  void update_scinfo(SR2ScanInfo*);

  bool update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t metric);

private:

  IPAddress _ip;    // My IP address.
  uint16_t _et;     // This protocol's ethertype
  unsigned int _period; // msecs
  unsigned int _jitter; // msecs
  unsigned int _expire; // msecs
  bool _debug;
  int _version;
  int *_channel;
	bool _busy;

  class SR2LinkTableMulti *_link_table;
  class AvailableInterfaces *_if_table;
  class ARPTableMulti *_arp_table;
	class SR2ChannelSelectorMulti *_ch_sel;

  Timer _timer;
  NTable _ntable;
	MCGraph _mcgraph;
	CHTable _chtable;
  
	void send(SR2ChannelAssignment);

//  static int write_handler(const String &, Element *, void *, ErrorHandler *);
//  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
