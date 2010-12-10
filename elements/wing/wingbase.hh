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

	bool update_link_table(Packet *);

	Packet * create_wing_packet(NodeAddress, NodeAddress, int, NodeAddress, NodeAddress, NodeAddress, int, PathMulti);
	Packet * encap(Packet *, PathMulti);
	void set_ethernet_header(WritablePacket *, NodeAddress src, NodeAddress dst);

protected:

	IPAddress _ip; // My address.

	class LinkTableMulti *_link_table;
	class ARPTableMulti *_arp_table;

	bool _debug;

};

CLICK_ENDDECLS
#endif
