#ifndef WINGHOPCOUNTMETRIC_HH
#define WINGHOPCOUNTMETRIC_HH
#include <click/element.hh>
#include "winglinkmetric.hh"
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <elements/wifi/bitrate.hh>
CLICK_DECLS

/*
 * =c
 * WINGHopCountMetric
 * =s Wifi
 * Hop Count metric.
 * 
 * =io
 * None
 *
 */

class WINGHopCountMetric: public WINGLinkMetric {

public:

	WINGHopCountMetric();
	~WINGHopCountMetric();
	const char *class_name() const { return "WINGHopCountMetric";	}
	void *cast(const char *);
	const char *processing() const { return AGNOSTIC; }

	void update_link(NodeAddress, NodeAddress, Vector<RateSize>, Vector<int> , Vector<int> , uint32_t , uint32_t);

};

CLICK_ENDDECLS
#endif
