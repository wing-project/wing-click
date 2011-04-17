#ifndef CLICK_DYNGW_HH
#define CLICK_DYNGW_HH
#include <click/element.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/hashtable.hh>
#include <click/ipaddress.hh>
#include "winggatewayselector.hh"
CLICK_DECLS

class DynGW : public Element { 

  public:

	DynGW();
	~DynGW();

	const char *class_name() const { return "DynGW"; }
	const char *port_count() const { return PORTS_0_0; }
	bool can_live_reconfigure() const { return true; }

	int configure(Vector<String> &, ErrorHandler *);

	bool enabled() { return _enabled; }

	/* handler stuff */
	void add_handlers();

	void fetch_hnas(Vector<HNAInfo> *);

  private:

	class AddressPair {
	  public:
		IPAddress _net;
		IPAddress _mask;
		AddressPair()
		  : _net(), _mask() {
		}
		AddressPair(IPAddress net, IPAddress mask)
		  : _net(net), _mask(mask) {
		}
		inline hashcode_t hashcode() const {
			return CLICK_NAME(hashcode)(_net) + CLICK_NAME(hashcode)(_mask);
		}
		inline bool operator==(AddressPair other) const {
			return (other._net == _net && other._mask == _mask);
		}
	};

	HashTable<AddressPair, bool> _allow;

	String _dev_name;
	IPAddress _ip; // My address.
	IPAddress _netmask; // My address.

	bool _enabled;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
