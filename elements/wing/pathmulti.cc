/*
 * pathmulti.{cc,hh}
 * Stefano Testi, Roberto Riggio
 *
 * Copyright (c) 2009 CREATE-NET
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
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

