#ifndef CLICK_WINGLINKMETRIC_HH
#define CLICK_WINGLINKMETRIC_HH
#include <click/element.hh>
#include "wingbase.hh"
#include "winglinkstat.hh"
#include <elements/wifi/bitrate.hh>
CLICK_DECLS

class WINGLinkMetric: public Element {
public:

	WINGLinkMetric();
	virtual ~WINGLinkMetric();

	const char *class_name() const { return "WINGLinkMetric"; }
	const char *processing() const { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *);

	virtual void update_link(NodeAddress, NodeAddress, Vector<RateSize>, Vector<int>, Vector<int>, uint32_t, uint32_t);

	void update_link_table(NodeAddress, NodeAddress, uint32_t, uint32_t, uint32_t, uint16_t);

protected:

	class LinkTableMulti *_link_table;
	bool _debug;

};

CLICK_ENDDECLS
#endif
