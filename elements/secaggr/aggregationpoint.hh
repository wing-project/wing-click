#ifndef CLICK_AGGREGATIONPOINT_HH
#define CLICK_AGGREGATIONPOINT_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include <click/timer.hh>
CLICK_DECLS

class AggregationPoint : public Element { 

  public:

	AggregationPoint();
	~AggregationPoint();

	const char *class_name() const		{ return "AggregationPoint"; }
	const char *port_count() const		{ return "-/1"; }
	const char *processing() const		{ return PULL_TO_PUSH; }

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);

	void run_timer(Timer *);

	void send_packed();
	void send_unpacked();

  protected:

	bool _debug;
	bool _pack;
	uint32_t _period;
	Timer _timer;
	NotifierSignal *_signals;

	class AvailableSensors *_as;

};

CLICK_ENDDECLS
#endif
