#ifndef CLICK_SENSOR_HH
#define CLICK_SENSOR_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

class Sensor : public Element { 

  public:
  
	Sensor();
	~Sensor();
  
	const char *class_name() const		{ return "Sensor"; }
	const char *port_count() const		{ return "-/1"; }
	const char *processing() const		{ return PULL; }

	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);

	Packet* pull(int port);

  private:

	enum { SAMPLING_SHIFT = 28 };
	enum { SAMPLING_MASK = (1 << SAMPLING_SHIFT) - 1 };

	bool _debug;
	uint8_t _sensor; 
	uint32_t _sampling_prob;
	uint32_t _keystream;
	uint32_t _value; 
	uint32_t _max_jitter;
	NotifierSignal *_signals;

	class AvailableSensors *_as;

};

CLICK_ENDDECLS
#endif
