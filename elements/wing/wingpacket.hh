#ifndef CLICK_WINGPACKET_HH
#define CLICK_WINGPACKET_HH
#include "pathmulti.hh"
#include "nodeaddress.hh"
#include "nodeairport.hh"
CLICK_DECLS

enum wing_packet_types { 
	WING_PT_DATA  = 0x01, // wing_data
	WING_PT_QUERY = 0x02, // wing_packet
	WING_PT_REPLY = 0x03, // wing_packet
	WING_PT_PROBE = 0x04, // wing_probe
	WING_PT_GATEWAY = 0x05, // wing_packet
	WING_PT_BCAST_DATA  = 0x06, // wing_bcast_data
};

enum wing_probe_type {
	PROBE_TYPE_LEGACY = 0x01,
	PROBE_TYPE_HT = 0x02,
};

static const uint8_t _wing_version = 0x21;
static const uint16_t _wing_et = 0x06AA;

/* header format */
CLICK_PACKED_STRUCTURE(
struct wing_header {,
	uint8_t _version; /* see protocol version */
	uint8_t _type;    /* see protocol type */
	uint16_t _cksum;
});

CLICK_PACKED_STRUCTURE(struct wing_link {,
	NodeAddress dep() { return NodeAddress(_dep_ip, _dep_iface); }
	void set_dep(NodeAddress dep)  { _dep_ip = dep._ip; _dep_iface = dep._iface; }
	NodeAddress arr() { return NodeAddress(_arr_ip, _arr_iface); }
	void set_arr(NodeAddress arr)  { _arr_ip = arr._ip; _arr_iface = arr._iface; }
  private:
	uint32_t _dep_ip;
	uint8_t _dep_iface;
	uint8_t _arr_iface;
	uint16_t _pad;
	uint32_t _arr_ip;
});

CLICK_PACKED_STRUCTURE(struct wing_link_ex {,
	NodeAddress dep() { return NodeAddress(_dep_ip, _dep_iface); }
	void set_dep(NodeAddress dep)  { _dep_ip = dep._ip; _dep_iface = dep._iface; }
	NodeAddress arr() { return NodeAddress(_arr_ip, _arr_iface); }
	void set_arr(NodeAddress arr)  { _arr_ip = arr._ip; _arr_iface = arr._iface; }
	uint16_t channel() { return ntohs(_channel); }
	void set_channel(uint16_t channel)  { _channel = htons(channel); }
	uint32_t fwd() { return ntohl(_fwd); }
	void set_fwd(uint32_t fwd)  { _fwd = htonl(fwd); }
	uint32_t rev() { return ntohl(_rev); }
	void set_rev(uint32_t rev)  { _rev = htonl(rev); }
	uint32_t seq() { return ntohl(_seq); }
	void set_seq(uint32_t seq)  { _seq = htonl(seq); }
	uint32_t age() { return ntohl(_age); }
	void set_age(uint32_t age)  { _age = htonl(age); }
  private:
	uint32_t _dep_ip;
	uint8_t _dep_iface;
	uint8_t _arr_iface;
	uint16_t _channel;
	uint32_t _fwd;
	uint32_t _rev;
	uint32_t _seq;
	uint32_t _age;
	uint32_t _arr_ip;
});

/* packet format */
CLICK_PACKED_STRUCTURE(
struct wing_packet : public wing_header {,
	/* packet length functions */
	static size_t len_wo_data(int nlinks) {
		return sizeof(struct wing_packet) + (nlinks) * (sizeof(wing_link_ex) - sizeof(uint32_t)) + sizeof(uint32_t);
	}
	size_t hlen_wo_data() const { return len_wo_data(_num_links); }
private:
	/* these are private and have access functions below so I
	 * don't have to remember about endianness
	 */
	uint8_t _num_links;
	uint8_t _next;  /* who should process this packet. */
	uint16_t _data_len;
	uint32_t _qdst; /* query destination */
	uint32_t _qsrc; /* query source */
	uint32_t _netmask; /* netmask */
	uint32_t _seq;
public:  
	uint8_t      num_links()                      { return _num_links; }
	uint8_t      next()                           { return _next; }
	IPAddress    qdst()                           { return IPAddress(_qdst); }
	IPAddress    qsrc()                           { return IPAddress(_qsrc); }
	IPAddress    netmask()                        { return IPAddress(_netmask); }
	uint32_t     seq()                            { return ntohl(_seq); }
	void         set_num_links(uint8_t num_links) { _num_links = num_links; }
	void         set_next(uint8_t next)           { _next = next; }
	void         set_qdst(IPAddress qdst)         { _qdst = qdst; }
	void         set_qsrc(IPAddress qsrc)         { _qsrc = qsrc; }
	void         set_netmask(IPAddress netmask)   { _netmask = netmask; }
	void         set_seq(uint32_t seq)            { _seq = htonl(seq); }
	/* remember that if you call this you must have 
	 * set the number of links in this packet!  
	 */
	void set_checksum() {
		_cksum = 0;
		_cksum = click_in_cksum((unsigned char *) this, hlen_wo_data());
	}	
	bool check_checksum() {
		return click_in_cksum((unsigned char *) this, hlen_wo_data()) == 0;
	}
	/* the rest of the packet is variable length based on _num_links. */
	wing_link_ex *get_link(int link) {
		uint32_t *ndx = (uint32_t *) (this + 1);
		ndx += link * (sizeof(wing_link_ex) - sizeof(uint32_t)) / sizeof(uint32_t);
		return (wing_link_ex *) ndx;
	}
	void set_link(int link, 
			NodeAddress dep, NodeAddress arr, 
			uint32_t fwd, uint32_t rev, 
			uint32_t seq, uint32_t age, 
			uint16_t channel) {
		wing_link_ex *lnk = get_link(link);
		lnk->set_dep(dep);
		lnk->set_arr(arr);
		lnk->set_channel(channel);
		lnk->set_fwd(fwd);
		lnk->set_rev(rev);
		lnk->set_seq(seq);
		lnk->set_age(age);
	}	
	uint16_t get_link_channel(int link) {
		return get_link(link)->channel();
	}
	uint32_t get_link_fwd(int link) {
		return get_link(link)->fwd();
	}
	uint32_t get_link_rev(int link) {
		return get_link(link)->rev();
	}
	uint32_t get_link_seq(int link) {
		return get_link(link)->seq();
	}
	uint32_t get_link_age(int link) {
		return get_link(link)->age();
	}
	NodeAddress get_link_arr(int link) {
		return get_link(link)->arr();
	}
	NodeAddress get_link_dep(int link) {
		return get_link(link)->dep();
	}
	PathMulti get_path() {
		PathMulti p;
		NodeAirport port = NodeAirport();
		for (int x = 0; x < num_links(); x++) {
			port.set_dep(get_link_dep(x));
			p.push_back(port);
			port = NodeAirport();
			port.set_arr(get_link_arr(x));
		}
		p.push_back(port);
		return p;
	}
});

/* data packet format */
CLICK_PACKED_STRUCTURE(
struct wing_data : public wing_header {,
	/* packet length functions */
	static size_t len_wo_data(int nlinks) {
		return sizeof(struct wing_data) + (nlinks) * (sizeof(wing_link) - sizeof(uint32_t)) + sizeof(uint32_t);
	}
	size_t hlen_wo_data()   const { return len_wo_data(_num_links); }
	size_t hlen_w_data()   const { return ntohs(_data_len) + len_wo_data(_num_links); }
private:
	/* these are private and have access functions below so I
	 * don't have to remember about endianness
	 */
	uint8_t _num_links;
	uint8_t _next;    /* who should process this packet. */
	uint16_t _data_len;
public:  
	uint8_t      num_links()                      { return _num_links; }
	uint8_t      next()                           { return _next; }
	uint16_t     data_len()                       { return ntohs(_data_len); }
	void         set_num_links(uint8_t num_links) { _num_links = num_links; }
	void         set_next(uint8_t next)           { _next = next; }
	void         set_data_len(uint16_t data_len)  { _data_len = htons(data_len); }
	/* remember that if you call this you must have set the number
	 * of links in this packet!
	 */
	u_char *data() { return (((u_char *)this) + hlen_wo_data()); }
	void set_checksum() {
		_cksum = 0;
		_cksum = click_in_cksum((unsigned char *) this, data_len() + hlen_wo_data());
	}	
	bool check_checksum() {
		return click_in_cksum((unsigned char *) this, data_len() + hlen_wo_data()) == 0;
	}
	/* the rest of the packet is variable length based on _num_links. */
	wing_link *get_link(int link) {
		uint32_t *ndx = (uint32_t *) (this + 1);
		ndx += link * (sizeof(wing_link) - sizeof(uint32_t)) / sizeof(uint32_t);
		return (wing_link *) ndx;
	}
	void set_link(int link, NodeAddress dep, NodeAddress arr) {
		wing_link *lnk = get_link(link);
		lnk->set_dep(dep);
		lnk->set_arr(arr);
	}	
	NodeAddress get_link_arr(int link) {
		return get_link(link)->arr();
	}
	NodeAddress get_link_dep(int link) {
		return get_link(link)->dep();
	}
	PathMulti get_path() {
		PathMulti p;
		NodeAirport port = NodeAirport();
		for (int x = 0; x < num_links(); x++) {
			port.set_dep(get_link_dep(x));
			p.push_back(port);
			port = NodeAirport();
			port.set_arr(get_link_arr(x));
		}
		p.push_back(port);
		return p;
	}
});

/* broadcast data packet format */
CLICK_PACKED_STRUCTURE(
struct wing_bcast_data : public wing_header {,
	/* packet length functions */
	size_t hlen_wo_data()   const { return sizeof(struct wing_bcast_data); }
	size_t hlen_w_data()   const { return ntohs(_data_len) + sizeof(struct wing_bcast_data); }
private:
	/* these are private and have access functions below so I
	 * don't have to remember about endianness
	 */
	uint16_t _data_len;
	uint16_t _pad;
public:  
	uint16_t     data_len()                       { return ntohs(_data_len); }
	void         set_data_len(uint16_t data_len)  { _data_len = htons(data_len); }
	u_char *data() { return (((u_char *)this) + sizeof(struct wing_bcast_data)); }
	void set_checksum() {
		_cksum = 0;
		_cksum = click_in_cksum((unsigned char *) this, data_len() + sizeof(struct wing_bcast_data));
	}	
	bool check_checksum() {
		return click_in_cksum((unsigned char *) this, data_len() + sizeof(struct wing_bcast_data)) == 0;
	}
});

/* probe packet format */
CLICK_PACKED_STRUCTURE(struct link_info {,
	uint16_t size() { return ntohs(_size); }
	uint16_t rate() { return ntohs(_rate); }
	uint32_t fwd()  { return ntohl(_fwd); }
	uint32_t rev()  { return ntohl(_rev); }
	void set_size(uint16_t size)  { _size = htons(size); }
	void set_rate(uint16_t rate)  { _rate = htons(rate); }
	void set_fwd(uint32_t fwd)    { _fwd = htonl(fwd); }
	void set_rev(uint32_t rev)    { _rev = htonl(rev); }
  private:
	uint16_t _size;
	uint16_t _rate;
	uint32_t _fwd;
	uint32_t _rev;
});

CLICK_PACKED_STRUCTURE(struct rate_entry {,
	uint32_t rate() { return ntohl(_rate); }
	void set_rate(uint32_t rate)  { _rate = htonl(rate); }
  private:
	uint32_t _rate;
});

CLICK_PACKED_STRUCTURE(struct link_entry {,
	NodeAddress node()     { return NodeAddress(_ip, _iface); }
	uint8_t num_rates()    { return _num_rates; }
	uint16_t channel()     { return ntohs(_channel); }
	uint32_t seq()         { return ntohl(_seq); }
	void set_node(NodeAddress node)         { _ip = node._ip; _iface = node._iface; }
	void set_num_rates(uint8_t num_rates)   { _num_rates = num_rates; }
	void set_channel(uint16_t channel)      { _channel = htons(channel); }
	void set_seq(uint32_t seq)              { _seq = htonl(seq); }
  private:
	uint32_t _ip;
	uint8_t _iface;
	uint8_t _num_rates;	// number of link_info entries
	uint16_t _channel;
	uint32_t _seq;
});

CLICK_PACKED_STRUCTURE(
struct wing_probe : public wing_header {,
	uint16_t rate()         { return ntohs(_rate); }
	uint16_t size()         { return ntohs(_size); }
	NodeAddress node()      { return NodeAddress(_ip, _iface); }
	uint16_t channel()      { return ntohs(_channel); }
	uint32_t seq()          { return ntohl(_seq); }
	uint32_t period()       { return ntohl(_period); }
	uint32_t tau()          { return ntohl(_tau); }
	uint32_t sent()         { return ntohl(_sent); }
	uint8_t num_probes()    { return _num_probes; }
	uint8_t num_links()     { return _num_links; }
	uint8_t num_rates()     { return _num_rates; }
	uint8_t rtype()         { return _rtype; }
	void set_rate(uint16_t rate) { _rate = htons(rate); }
	void set_size(uint16_t size) { _size = htons(size); }
	void set_node(NodeAddress node)              { _ip = node._ip; _iface = node._iface; }
	void set_period(uint32_t period)             { _period = htonl(period); }
	void set_channel(uint16_t channel)           { _channel = htons(channel); }
	void set_seq(uint32_t seq)                   { _seq = htonl(seq); }
	void set_tau(uint32_t tau)                   { _tau = htonl(tau); }
	void set_sent(uint32_t sent)                 { _sent = htonl(sent); }
	void set_num_probes(uint8_t num_probes)      { _num_probes = num_probes; }
	void set_num_links(uint8_t num_links)        { _num_links = num_links; }
	void set_num_rates(uint8_t num_rates)        { _num_rates = num_rates; }
	void set_rtype(uint8_t rtype)                { _rtype = rtype; }
	/* remeber to set size before calling this */
	void set_checksum() {
		_cksum = 0;
		_cksum = click_in_cksum((unsigned char *) this, size());
	}	
	bool check_checksum() {
		return click_in_cksum((unsigned char *) this, size()) == 0;
	}
  private:
	uint16_t _rate;
	uint16_t _size;
	uint8_t _iface;
	uint8_t _rtype;
	uint16_t _channel;
	uint32_t _ip;
	uint32_t _period;      // period of this node's probe broadcasts, in msecs
	uint32_t _tau;         // this node's loss-rate averaging period, in msecs
	uint32_t _sent;        // how many probes this node has sent
	uint32_t _seq;
	uint8_t _num_probes;  
	uint8_t _num_links;    // number of link_entry entries following
	uint8_t _num_rates;    // number of rate_entry or entries following
	uint8_t _pad; 
});

CLICK_ENDDECLS
#endif /* CLICK_WINGPACKET_HH */
