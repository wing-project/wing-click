#ifndef CLICK_SETTXRATEHT_HH
#define CLICK_SETTXRATEHT_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * SetTXRateHT([I<KEYWORDS>])
 * =s Wifi
 * Sets the MCS index for a packet.
 * =d
 * Sets the Wifi MCS Annotation on a packet.
 * Regular Arguments:
 * =over 8
 * =item MCS
 * Unsigned integer. The mcs field indicates the MCS rate 
 * index as in IEEE_802.11n-2009
 * =back 8
 * =h rate read/write
 * Same as MCS Argument
 * =a AutoRateFallback, MadwifiRate, ProbeRate, ExtraEncap
*/

class SetTXRateHT : public Element { 

  public:

	SetTXRateHT();
	~SetTXRateHT();

	const char *class_name() const		{ return "SetTXRateHT"; }
	const char *port_count() const		{ return PORTS_1_1; }
	const char *processing() const		{ return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);
	bool can_live_reconfigure() const		{ return true; }

	Packet *simple_action(Packet *);

	void add_handlers();
	static String read_handler(Element *e, void *);
	static int write_handler(const String &arg, Element *e, void *, ErrorHandler *errh);

  private:

	int _mcs;
	int _tries;
	uint16_t _et;
	unsigned _offset;

};

CLICK_ENDDECLS
#endif
