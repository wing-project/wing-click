#ifndef CLICK_PATHMULTI_HH
#define CLICK_PATHMULTI_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
#include <click/vector.hh>
#include "nodeairport.hh"
CLICK_DECLS

typedef Vector<NodeAirport> PathMulti;

template<>
inline hashcode_t hashcode(const PathMulti &p) {
	hashcode_t h = 0;
	for (int x = 0; x < p.size(); x++)
		h ^= CLICK_NAME(hashcode)(p[x]);
	return h;
}

inline bool operator==(const PathMulti &p1, const PathMulti &p2) {
	if (p1.size() != p2.size()) {
		return false;
	}
	for (int x = 0; x < p1.size(); x++) {
		if (p1[x] != p2[x]) {
			return false;
		}
	}
	return true;
}

inline bool operator!=(const PathMulti &p1, const PathMulti &p2) {
	return (!(p1 == p2));
}

StringAccum & operator<<(StringAccum &sa, PathMulti p);

inline String route_to_string(const PathMulti &p) {
	StringAccum sa;
	for (int x = 0; x < p.size(); x++) {
		sa << p[x].unparse();
		if (x != p.size() - 1) {
			sa << " ";
		}
	}
	return sa.take_string();
}

CLICK_ENDDECLS
#endif /* CLICK_PATHMULTI_HH */
