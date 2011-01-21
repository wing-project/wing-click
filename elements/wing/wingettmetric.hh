#ifndef WINGETTMETRIC_HH
#define WINGETTMETRIC_HH
#include <click/element.hh>
#include "winglinkmetric.hh"
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <elements/wifi/bitrate.hh>
CLICK_DECLS

/*
 * =c
 * WINGETTMetric
 * =s Wifi
 * Estimated Transmission Time (ETT) metric
 * =a WINGETXMetric, WINGHopCountMetric
 */

class WINGETTMetric: public WINGLinkMetric {

public:

	WINGETTMetric();
	~WINGETTMetric();
	const char *class_name() const { return "WINGETTMetric";	}
	void *cast(const char *);
	const char *processing() const { return AGNOSTIC; }

	inline unsigned compute_metric(int ack_prob, int data_prob, int data_rate) {

		if (!ack_prob || !data_prob || ack_prob < 30 || data_prob < 30) {
			return 999999;
		}

		int retries = 100 * 100 * 100 / (ack_prob * data_prob) - 100;
		unsigned low_usecs = calc_usecs_wifi_packet(1500, data_rate, retries / 100);
		unsigned high_usecs = calc_usecs_wifi_packet(1500, data_rate, retries / 100 + 1);

		unsigned diff = retries % 100;
		unsigned average = (diff * high_usecs + (100 - diff) * low_usecs) / 100;
		return average;

	}

	void update_link(NodeAddress, NodeAddress, Vector<RateSize>, Vector<int> , Vector<int> , uint32_t , uint32_t);
 
};

CLICK_ENDDECLS
#endif
