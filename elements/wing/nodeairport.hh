#ifndef CLICK_NODEAIRPORT_HH
#define CLICK_NODEAIRPORT_HH
#include <click/config.h>
#include <click/straccum.hh>
#include "nodeaddress.hh"
CLICK_DECLS

class NodeAirport{
public:

	IPAddress _ip;
	uint8_t _arr;
	uint8_t _dep;
	
	NodeAirport() : _ip(IPAddress()), _arr(0), _dep(0) {}
	NodeAirport(IPAddress ip, uint8_t arr, uint8_t dep) : _ip(ip), _arr(arr), _dep(dep) {}

	void set_arr(NodeAddress p) { _ip = p._ip; _arr = p._iface; }
	void set_dep(NodeAddress p) { _ip = p._ip; _dep = p._iface; }
	NodeAddress arr() { return NodeAddress(_ip, _arr); }
	NodeAddress dep() { return NodeAddress(_ip, _dep); }

	typedef uint32_t (NodeAirport::*unspecified_bool_type)() const;
	/** @brief Return true if the address is not 0.0.0.0. */
	inline operator unspecified_bool_type() const {
		return _ip.addr() != 0 ? &NodeAirport::addr : 0;
	}

	/** @brief Return the address as a uint32_t in network byte order. */
	inline operator uint32_t() const {
		return _ip.addr();
	}

	/** @brief Return the address as a uint32_t in network byte order. */
	inline uint32_t addr() const {
		return _ip.addr();
	}

	inline uint32_t hashcode() const {
		return _ip.addr();
	}

	inline bool operator==(NodeAirport other) const {
		return (other._ip == _ip && other._arr == _arr && other._dep == _dep);
    	}

	inline bool operator!=(NodeAirport other) const {
		return (other._ip != _ip || other._arr != _arr || other._dep != _dep);
    	}
	
	String unparse() const {
		StringAccum sa;
		sa << String((int)_arr) << ':' << _ip.unparse() << ':' << String((int)_dep);
		return sa.take_string();
	}
};

CLICK_ENDDECLS
#endif /* CLICK_NODEAIRPORT_HH */
