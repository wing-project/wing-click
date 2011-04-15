#ifndef CLICK_DYNGW_HH
#define CLICK_DYNGW_HH
#include <click/element.hh>
#include <click/error.hh>
#include <click/timer.hh>
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
	int initialize(ErrorHandler *);
	void run_timer(Timer *);

	/* handler stuff */
	void add_handlers();

  private:

	class WINGGatewaySelector *_gw_sel;

	bool _debug;
	String _dev_name;
	unsigned int _period; // msecs

	void refresh_hnas();

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

	Timer _timer;

};

CLICK_ENDDECLS
#endif
