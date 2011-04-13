#include <click/config.h>
#include <click/confparse.hh>
#include "pathmulti.hh"
CLICK_DECLS

StringAccum & operator<<(StringAccum &sa, PathMulti p) {
	if (p.size() == 0) {
		sa << "(empty route)";
		return sa;
	}
	for (int x = 0; x < p.size(); x++) {
		sa << p[x].unparse();
		if (x != p.size() - 1) {
			sa << " ";
		}
	}
	return sa;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(PathMulti)

