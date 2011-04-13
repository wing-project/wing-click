#ifndef CLICK_NODEADDRESS_HH
#define CLICK_NODEADDRESS_HH
#include <click/ipaddress.hh>
#include <click/straccum.hh>
CLICK_DECLS

class NodeAddress { public:

	IPAddress _ip;
	uint8_t _iface;

	NodeAddress() :
		_ip(IPAddress()), _iface(0) {
	}

	NodeAddress(IPAddress ip) :
		_ip(ip), _iface(0) {
	}

	NodeAddress(IPAddress ip, uint8_t iface) :
		_ip(ip), _iface(iface) {
	}

	typedef uint32_t (NodeAddress::*unspecified_bool_type)() const;
	/** @brief Return true if the address is not 0.0.0.0. */
	inline operator unspecified_bool_type() const {
		return _ip.addr() != 0 ? &NodeAddress::addr : 0;
	}

	/** @brief Return the address as a uint32_t in network byte order. */
	inline operator uint32_t() const {
		return _ip.addr();
	}

	/** @brief Return the address as a uint32_t in network byte order. */
	inline uint32_t addr() const {
		return _ip.addr();
	}

	inline operator IPAddress() { 
		return _ip; 
	} 

	inline uint32_t hashcode() const {
		return _ip.addr();
	}

	inline bool operator==(NodeAddress other) const {
		return (other._ip == _ip && other._iface == _iface);
	}

	inline bool operator!=(NodeAddress other) const {
		return (other._ip != _ip || other._iface != _iface);
	}

	String unparse() const {
		StringAccum sa;
		sa << _ip.unparse() << ':' << String((int)_iface);
		return sa.take_string();
	}
};

StringAccum & operator<<(StringAccum &, NodeAddress);

CLICK_ENDDECLS
#endif 
