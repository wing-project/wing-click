#ifndef CLICK_DYNGW_HH
#define CLICK_DYNGW_HH
#include <click/element.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/hashtable.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

class DynGW : public Element { 

  public:

	DynGW();
	~DynGW();

	const char *class_name() const { return "DynGW"; }
	const char *port_count() const { return PORTS_0_0; }
	bool can_live_reconfigure() const { return true; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);

	bool enabled() { return _enabled; }

	void run_timer(Timer *);

	/* handler stuff */
	void add_handlers();

  private:

	class WINGGatewaySelector *_sel;

	String _dev_name;

	unsigned int _period; // msecs
	bool _enabled;
	bool _default_route;

	Timer _timer;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

	void default_route(String);

};

CLICK_ENDDECLS
#endif
