#ifndef WINGHOPCOUNTMETRIC_HH
#define WINGHOPCOUNTMETRIC_HH
#include <click/element.hh>
#include "winglinkmetric.hh"
CLICK_DECLS

/*
 * =c
 * WINGHopCountMetric
 * =s Wifi
 * Estimated Transmission Time (ETT) metric
 * =a WINGETXMetric, WINGETTMetric
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
