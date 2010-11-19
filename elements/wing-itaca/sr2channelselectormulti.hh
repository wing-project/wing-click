#ifndef CLICK_SR2CHANNELSELECTORMULTI_HH
#define CLICK_SR2CHANNELSELECTORMULTI_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/wifi/path.hh>
CLICK_DECLS

/*
 * =c
 * SR2ChannelSelectorMulti(IP, ETH, ETHTYPE, LinkTable element, ARPTable element,  
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

class SR2ChannelAssignment {
  public:
    NodeAddress _node;
    uint16_t _new_iface;
    SR2ChannelAssignment() : _node(), _new_iface() { }
    SR2ChannelAssignment(NodeAddress node, int new_iface) {
      _node = node;
      _new_iface = new_iface;
    }
    void update (NodeAddress node, int new_iface) {
      _node = node;
      _new_iface = new_iface;
    }
};

class SR2ChannelChangeWarning {
 public:
   SR2ChannelAssignment _ch_ass;
   bool _start;
	 bool _scan;
   SR2ChannelChangeWarning() : _ch_ass(), _start(), _scan() { }
   SR2ChannelChangeWarning(SR2ChannelAssignment ch_ass, bool start, bool scan) {
     _ch_ass = ch_ass;
     _start = start;
		 _scan = scan;
   }
};

class SR2ChannelInfo {
  public:
		HashMap<int,int> _ctable;
		SR2ChannelInfo() {
			_ctable.clear();
		}
		void clear () {
			_ctable.clear();
    }
    void update (int channel, int psize) {
			int* value = _ctable.findp(channel);
			if (!value){
				_ctable.insert(channel,psize);
			} else {
				*value = psize;
			}
    }
 };

class SR2ScanInfo {
	public:
		NodeAddress _node;
		uint16_t _version;
		HashMap<int,int> _channel_table;
		HashMap<NodeAddress,int> _neighbors_table;
		uint16_t _cas_hops;
		SR2ScanInfo() {
			_node = NodeAddress();
			_version = 0;
			_channel_table.clear();
			_neighbors_table.clear();
			_cas_hops = 0;
		}
		SR2ScanInfo(NodeAddress node, int version, SR2ChannelInfo channel_table, HashMap<NodeAddress,int> neighbors_table, int cas_hops) {
			_node = node;
			_version = version;
			for (HashMap<int,int>::const_iterator iter = channel_table._ctable.begin(); iter.live(); iter++){
		    _channel_table.insert(iter.key(), iter.value());
		  }
			for (HashMap<NodeAddress,int>::const_iterator iter = neighbors_table.begin(); iter.live(); iter++){
		    _neighbors_table.insert(iter.key(), iter.value());
		  }
			_cas_hops = cas_hops;
		}
 };

class SR2ChannelSelectorMulti : public Element {
 public:
  
  SR2ChannelSelectorMulti();
  ~SR2ChannelSelectorMulti();
  
  const char *class_name() const		{ return "SR2ChannelSelectorMulti"; }
  const char *port_count() const		{ return "1/2"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  
  /* handler stuff */
  void add_handlers();
  String print_cas_stats();

  void push(int, Packet *);
  void run_timer(Timer *);

  IPAddress best_cas();
  bool is_cas() { return _is_cas; }

  bool update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t metric);

	void set_local_scinfo(SR2ScanInfo);
	HashMap<int,SR2ScanInfo> get_local_scinfo();

  
  void send_change_warning(bool, bool, SR2ChannelAssignment);
  void handle_change_warning(bool, bool, SR2ChannelAssignment);
  void switch_channel(bool, SR2ChannelAssignment);
  static void static_forward_ad_hook(Timer *, void *e) { 
    ((SR2ChannelSelectorMulti *) e)->forward_ad_hook(); 
  }

private:

  // List of query sequence #s that we've already seen.
  class Seen {
   public:
    Seen();
    Seen(IPAddress cas, u_long seq, int fwd, int rev) {
	_cas = cas; 
	_seq = seq; 
	_count = 0;
	(void) fwd, (void) rev;
    }
    IPAddress _cas;
    uint32_t _seq;
    int _count;
    Timestamp _when; /* when we saw the first query */
    Timestamp _to_send;
    bool _forwarded;
  };
  
  DEQueue<Seen> _seen;

  class CASInfo {
  public:
    CASInfo() {memset(this,0,sizeof(*this)); }
    IPAddress _ip;
    Timestamp _first_update;
    Timestamp _last_update;
    int _seen;
  };

  typedef HashMap<IPAddress, CASInfo> CASTable;
  typedef CASTable::const_iterator CASIter;

  CASTable _castable;

  typedef HashMap<IPAddress, IPAddress> IPTable;
  typedef IPTable::const_iterator IPIter;

  IPTable _ignore;
  IPTable _allow;

	HashMap<int,SR2ScanInfo> _local_scinfo;

  bool _is_cas;
  uint32_t _seq;      // Next query sequence number to use.
  IPAddress _ip;    // My IP address.
  uint16_t _et;     // This protocol's ethertype

	bool _debug;

  unsigned int _period; // msecs
  unsigned int _jitter; // msecs
  unsigned int _expire; // msecs

  class SR2LinkTableMulti *_link_table;
  class AvailableInterfaces *_if_table;
  class ARPTableMulti *_arp_table;

  Timer _timer;

  void start_ad();
  void send(WritablePacket *, EtherAddress);
  void forward_ad(Seen *s);
  void forward_ad_hook();
  void cleanup();

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
