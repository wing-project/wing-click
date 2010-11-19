#ifndef CLICK_CHANNELTABLE_HH
#define CLICK_CHANNELTABLE_HH
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/ewma.hh>
#include <click/timer.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

class ChannelTable : public Element { public:

	ChannelTable();
	~ChannelTable();

	const char *class_name() const		{ return "ChannelTable"; }
	const char *port_count() const		{ return "1/1"; }
	const char *processing() const		{ return AGNOSTIC; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &conf, ErrorHandler *errh);
	void run_timer(Timer *);

	void add_handlers();
	String print_stats();

	Packet *simple_action(Packet *);

private:

	class ChnInfo {
	public:
		ChnInfo() {}
		uint32_t _utilization;
		DirectEWMAX<FixedEWMAXParameters<3, 20, uint32_t, int32_t> > _ewma_utilization;
		uint32_t _interfering;
		DirectEWMAX<FixedEWMAXParameters<3, 20, uint32_t, int32_t> > _ewma_interfering;
	};

	typedef HashMap<int, ChnInfo> ChnTable;
	typedef ChnTable::const_iterator ChnIter;

	typedef HashMap<EtherAddress, uint32_t> IntTable;
	typedef IntTable::const_iterator IntIter;

	IntTable _interfering;
	ChnTable _channels;
	Timer _timer;

	class DevInfo *_dev;
	class ARPTableMulti *_arp_table;

	bool _scanning;
	int _old_channel;
	int _next_channel;
	uint32_t _utilization_counter;
	uint32_t _period;
	uint32_t _parking;
	Timestamp _last_updated;

	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
