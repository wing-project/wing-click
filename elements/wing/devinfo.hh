#ifndef CLICK_DEVINFO_HH
#define CLICK_DEVINFO_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

class DevInfo: public Element {
public:

	DevInfo();
	~DevInfo();

	const char *class_name() const { return "DevInfo"; }
	const char *port_count() const { return PORTS_0_0; }

	int configure(Vector<String> &, ErrorHandler *);

	/* handler stuff */
	void add_handlers();
	EtherAddress eth() { return _eth; }
	uint16_t channel() { return _channel; }
	void set_channel(uint16_t channel) {
		if (_debug) {
			click_chatter("%{element} :: %s :: %s switching channel to %u",
					this, 
					__func__, 
					_eth.unparse().c_str(),
					channel);
		}
		_channel = channel; 
	}

private:

	bool _debug;
	EtherAddress _eth;
	uint32_t _channel;
	
	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

	class AvailableChannels *_ctable;
	class AvailableRates *_rtable;
	
public:

	const AvailableRates* rtable() { return _rtable; }
	const AvailableChannels* ctable() { return _ctable; }

};

CLICK_ENDDECLS
#endif
