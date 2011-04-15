#ifndef CLICK_DYNGW_HH
#define CLICK_DYNGW_HH
#include <click/element.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/vector.hh>
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

	String _dev_name;
	IPAddress _ip; // My address.
	IPAddress _netmask; // My address.

	bool _enabled;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
