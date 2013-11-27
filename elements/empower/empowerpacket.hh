// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERPACKET_HH
#define CLICK_EMPOWERPACKET_HH
#include <click/config.h>
#include <click/etheraddress64.hh>
CLICK_DECLS

/* protocol version */
static const uint8_t _empower_version = 0x00;

/* protocol type */
enum empower_packet_types {
	EMPOWER_PT_HELLO = 0x01,          // wtp -> ac
	EMPOWER_PT_PROBE_REQUEST = 0x02,  // wtp -> ac
	EMPOWER_PT_PROBE_RESPONSE = 0x03, // ac -> wtp
	EMPOWER_PT_AUTH_REQUEST = 0x04,   // wtp -> ac
	EMPOWER_PT_AUTH_RESPONSE = 0x05,  // ac -> wtp
	EMPOWER_PT_ASSOC_REQUEST = 0x06,  // wtp -> ac
	EMPOWER_PT_ASSOC_RESPONSE = 0x07, // ac -> wtp
	EMPOWER_PT_ADD_LVAP = 0x08,       // ac -> wtp
	EMPOWER_PT_DEL_LVAP = 0x09,       // ac -> wtp
	EMPOWER_PT_STATUS_LVAP = 0x10     // wtp -> ac
};

enum empower_packet_flags {
	EMPOWER_STATUS_LVAP_AUTHENTICATED = (1<<0),
	EMPOWER_STATUS_LVAP_ASSOCIATED = (1<<1),
};

/* header format, common to all messages */
CLICK_PACKED_STRUCTURE(
struct empower_header {,
  private:
	uint8_t _version; /* see protocol version */
	uint8_t _type;    /* see protocol type */
	uint16_t _length; /* including this header */
	uint32_t _seq;    /* sequence number, increased by one for
	                   * each message sent to ANY WTP
	                   */
  public:
	uint8_t      version()                        { return _version; }
	uint8_t      type()                           { return _type; }
	uint16_t     length()                         { return ntohs(_length); }
	uint32_t     seq()                            { return ntohl(_seq); }
	void         set_seq(uint32_t seq)            { _seq = htonl(seq); }
	void         set_version(uint8_t version)     { _version = version; }
	void         set_type(uint8_t type)           { _type = type; }
	void         set_length(uint16_t length)      { _length = htons(length); }
});

/* hello packet format */
CLICK_PACKED_STRUCTURE(
struct empower_hello : public empower_header {,
  private:
	uint32_t _port;  /* OVS port id to which the WTP generating
	                  * this message is attached
	                  */
	uint8_t	_wtp[6];  /* WTP EtherAddress */
	uint8_t	_dpid[8]; /* DPID EtherAddress */
  public:
	EtherAddress   wtp()							  { return EtherAddress(_wtp); }
	EtherAddress64 dpid()							  { return EtherAddress64(_dpid); }
	uint32_t       port()                             { return ntohl(_port); }
	void           set_port(uint32_t port)            { _port = htonl(port); }
	void           set_wtp(EtherAddress wtp)		  { memcpy(_wtp, wtp.data(), 6); }
	void           set_dpid(EtherAddress64 dpid)	  { memcpy(_dpid, dpid.data(), 8); }
});

/* probe request packet format */
CLICK_PACKED_STRUCTURE(
struct empower_probe_request : public empower_header {,
  private:
    uint8_t	_wtp[6];
    uint8_t	_sta[6];
    char _ssid[];
  public:
	EtherAddress wtp()							  { return EtherAddress(_wtp); }
	EtherAddress sta()							  { return EtherAddress(_sta); }
	void         set_wtp(EtherAddress wtp)		  { memcpy(_wtp, wtp.data(), 6); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
	void         set_ssid(String ssid)		      { memcpy(&_ssid, ssid.data(), ssid.length()); }
});

/* probe response packet format */
CLICK_PACKED_STRUCTURE(
struct empower_probe_response : public empower_header {,
  private:
    uint8_t	_sta[6];
  public:
	EtherAddress sta()							  { return EtherAddress(_sta); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
});

/* auth request packet format */
CLICK_PACKED_STRUCTURE(
struct empower_auth_request : public empower_header {,
  private:
    uint8_t	_wtp[6];
    uint8_t	_sta[6];
  public:
	EtherAddress wtp()							  { return EtherAddress(_wtp); }
	EtherAddress sta()							  { return EtherAddress(_sta); }
	void         set_wtp(EtherAddress wtp)		  { memcpy(_wtp, wtp.data(), 6); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
});

/* auth response packet format */
CLICK_PACKED_STRUCTURE(
struct empower_auth_response : public empower_header {,
private:
  uint8_t	_sta[6];
public:
	EtherAddress sta()							  { return EtherAddress(_sta); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
});

/* association request packet format */
CLICK_PACKED_STRUCTURE(
struct empower_assoc_request : public empower_header {,
  private:
    uint8_t	_wtp[6];
    uint8_t	_sta[6];
    char _ssid[];
  public:
	EtherAddress wtp()							  { return EtherAddress(_wtp); }
	EtherAddress sta()							  { return EtherAddress(_sta); }
	void         set_wtp(EtherAddress wtp)		  { memcpy(_wtp, wtp.data(), 6); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
	void         set_ssid(String ssid)		      { memcpy(&_ssid, ssid.data(), ssid.length()); }
});

/* association response packet format */
CLICK_PACKED_STRUCTURE(
struct empower_assoc_response : public empower_header {,
  private:
    uint8_t	_sta[6];
  public:
	EtherAddress sta()							  { return EtherAddress(_sta); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
});

/* add vap packet format */
CLICK_PACKED_STRUCTURE(
struct empower_add_lvap : public empower_header {,

private:

	uint16_t _flags;
	uint16_t _assoc_id;
	uint8_t	_sta[6];
	uint8_t	_bssid[6];
    // { u8 length, SSID }*

public:

	bool      flag(int f)                 { return ntohs(_flags) & f;  }
	uint16_t  assoid()                    { return ntohs(_assoc_id); }
	EtherAddress sta()					  { return EtherAddress(_sta); }
	EtherAddress bssid()				  { return EtherAddress(_bssid); }

	void      set_flag(uint16_t f)          { _flags = htons(ntohs(_flags) | f); }
	void      set_assoid(uint16_t assoc_id) { _assoc_id = htons(assoc_id); }
	void      set_sta(EtherAddress sta)		{ memcpy(_sta, sta.data(), 6); }
	void      set_bssid(EtherAddress bssid)	{ memcpy(_bssid, bssid.data(), 6); }

});

/* del vap packet format */
CLICK_PACKED_STRUCTURE(
struct empower_del_lvap : public empower_header {,
  private:
    uint8_t	_sta[6];
  public:
	EtherAddress sta()							  { return EtherAddress(_sta); }
	void         set_sta(EtherAddress sta)		  { memcpy(_sta, sta.data(), 6); }
});

/* lvap status packet format */
CLICK_PACKED_STRUCTURE(
struct empower_status_lvap : public empower_header {,


private:

	uint16_t _flags;
	uint16_t _assoc_id;
	uint8_t	_wtp[6];
	uint8_t	_sta[6];
	uint8_t	_bssid[6];
    // { u8 length, SSID }*

public:

	bool      flag(int f)                 { return ntohs(_flags) & f;  }
	uint16_t  assoid()                    { return ntohs(_assoc_id); }
	EtherAddress wtp()					  { return EtherAddress(_wtp); }
	EtherAddress sta()					  { return EtherAddress(_sta); }
	EtherAddress bssid()				  { return EtherAddress(_bssid); }

	void      set_flag(uint16_t f)          { _flags = htons(ntohs(_flags) | f); }
	void      set_assoid(uint16_t assoc_id) { _assoc_id = htons(assoc_id); }
	void      set_wtp(EtherAddress wtp)		{ memcpy(_wtp, wtp.data(), 6); }
	void      set_sta(EtherAddress sta)		{ memcpy(_sta, sta.data(), 6); }
	void      set_bssid(EtherAddress bssid)	{ memcpy(_bssid, bssid.data(), 6); }

});

CLICK_ENDDECLS
#endif /* CLICK_EMPOWERPACKET_HH */
