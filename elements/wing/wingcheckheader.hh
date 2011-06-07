#ifndef CLICK_WINGCHECKHEADER_HH
#define CLICK_WINGCHECKHEADER_HH
#include <click/element.hh>
#include "wingbase.hh"
CLICK_DECLS

/*
 * =c
 * WINGCheckHeader([CHECKSUM])
 * =s Wifi, Wireless Routing
 * Check the Source Route Header of a packet.
 * =d
 * Expects Wing packets as input. Checks that the packet's 
 * length is reasonable, and that the Wing header length, 
 * length, and checksum fields are valid. 
 * =a WINGSetHeader
 */

class WINGCheckHeader: public Element {

public:

	WINGCheckHeader();
	~WINGCheckHeader();

	const char *class_name() const { return "WINGCheckHeader"; }
	const char *port_count() const { return "1/1-2"; }
	const char *processing() const { return "a/ah"; }

	void add_handlers();

	Packet *simple_action(Packet *);

	int drops() const {
		return _drops;
	}

	String bad_nodes();

private:

	typedef HashMap<EtherAddress, uint8_t> BadTable;
	typedef BadTable::const_iterator BTIter;

	BadTable _bad_table;
	int _drops;

	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
