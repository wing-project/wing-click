#ifndef WINGETXMETRIC_HH
#define WINGETXMETRIC_HH
#include <click/element.hh>
#include "winglinkmetric.hh"
CLICK_DECLS

/*
 * =c
 * WINGETXMetric
 * =s Wifi
 * Estimated Transmission Count (ETX) metric
 * =a WINGETTMetric, WINGHopCountMetric
 */

class WINGETXMetric: public WINGLinkMetric {

public:

	WINGETXMetric();
	~WINGETXMetric();
	const char *class_name() const { return "WINGETXMetric";	}
	void *cast(const char *);
	const char *processing() const { return AGNOSTIC; }

	inline unsigned compute_metric(int ack_prob, int data_prob) {
		if (!ack_prob || !data_prob || ack_prob < 30 || data_prob < 30) {
			return 999999;
		}
		return 100 * 100 * 100 / (ack_prob * data_prob) - 100;
	}	

	void update_link(NodeAddress, NodeAddress, Vector<RateSize>, Vector<int> , Vector<int> , uint32_t , uint32_t);

};

CLICK_ENDDECLS
#endif
