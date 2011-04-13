#include <click/config.h>
#include <click/confparse.hh>
#include "nodeaddress.hh"
CLICK_DECLS

StringAccum &
operator<<(StringAccum &sa, NodeAddress na) {
	const unsigned char *p = na._ip.data();
	char buf[20];
	int amt;
	amt = sprintf(buf, "%d.%d.%d.%d:%u", p[0], p[1], p[2], p[3], na._iface);
	sa.append(buf, amt);
	return sa;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(NodeAddress)

