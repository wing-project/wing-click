#ifndef CLICK_WINGBASE_HH
#define CLICK_WINGBASE_HH
#include <click/element.hh>
#include "winglinkstat.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
CLICK_DECLS

class WINGBase: public Element {
public:

	WINGBase();
	virtual ~WINGBase();

	const char *class_name() const { return "WINGBase"; }
	const char *processing() const { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);

	bool update_link_table(wing_packet *);

	Packet * encap(NodeAddress, NodeAddress, int, NodeAddress, NodeAddress, NodeAddress, int, PathMulti);

protected:

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	bool _debug;

};

CLICK_ENDDECLS
#endif
