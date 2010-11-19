#ifndef CLICK_AVAILABLECHANNELS_HH
#define CLICK_AVAILABLECHANNELS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

class AvailableChannels: public Element {
public:

	AvailableChannels();
	~AvailableChannels();

	const char *class_name() const { return "AvailableChannels"; }
	const char *port_count() const { return PORTS_0_0; }
	bool can_live_reconfigure() const { return true; }

	int configure(Vector<String> &, ErrorHandler *);

	/* handler stuff */
	void add_handlers();
	void take_state(Element *e, ErrorHandler *);

	Vector<int> lookup(EtherAddress eth);
	int insert(EtherAddress eth, Vector<int> );

private:

	bool _debug;

	int parse_and_insert(String s, ErrorHandler *errh);

	class DstInfo {
	public:
		EtherAddress _eth;
		Vector<int> _channels;
		DstInfo() {}
		DstInfo(EtherAddress eth) : _eth(eth) {}
	};

	typedef HashMap<EtherAddress, DstInfo> RTable;
	typedef RTable::const_iterator RIter;

	RTable _rtable;
	Vector<int> _default_channels;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
