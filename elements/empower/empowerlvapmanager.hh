// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_EMPOWERLVAPMANAGER_HH
#define CLICK_EMPOWERLVAPMANAGER_HH
#include <click/config.h>
#include <click/element.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
CLICK_DECLS

/*
=c

EmpowerLVAPManager(HWADDR, EBS, DEBUGFS[, I<KEYWORDS>])

=s EmPOWER

Handles LVAPs and communication with the Access Controller.

=d

Keyword arguments are:

=over 8

=item HWADDR
The raw wireless interface hardware address

=item EBS
An EmpowerBeaconSource element

=item EAUTHR
An EmpowerOpenAuthResponder element

=item EASSOR
An EmpowerAssociationResponder element

=item DEBUGFS
The path to the bssid_extra file

=item EPSB
An EmpowerPowerSaveBuffer element

=item PERIOD
Interval between hello messages to the Access Controller (in msec), default is 5000

=item DEBUG
Turn debug on/off

=back 8

=a EmpowerLVAPManager
*/

class EmpowerStationState {
public:
	EtherAddress _bssid;
	Vector<String> _ssids;
	String _ssid;
	int _assoc_id;
	bool _power_save;
	bool _authentication_status;
	bool _association_status;
};

typedef HashTable<EtherAddress, EmpowerStationState> LVAP;
typedef LVAP::iterator LVAPSIter;

class EmpowerLVAPManager: public Element {
public:

	EmpowerLVAPManager();
	~EmpowerLVAPManager();

	const char *class_name() const { return "EmpowerLVAPManager"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	void run_timer(Timer *);
	void reset();

	// Methods to add/remove VAPs.
	int add_lvap(EtherAddress, EtherAddress, String, Vector<String>, int, bool, bool);
	int del_lvap(EtherAddress);

	void push(int, Packet *);

	int handle_add_lvap(Packet *, uint32_t);
	int handle_del_lvap(Packet *, uint32_t);
	int handle_probe_response(Packet *, uint32_t);
	int handle_auth_response(Packet *, uint32_t);
	int handle_assoc_response(Packet *, uint32_t);

	void send_hello();
	void send_probe_request(EtherAddress, String);
	void send_auth_request(EtherAddress);
	void send_association_request(EtherAddress, String);
	void send_status_lvap(EtherAddress);

	LVAP* lvaps() { return &_lvaps; }
	uint32_t get_next_seq() { return ++_seq; }

private:

	void compute_bssid_mask();

	class EmpowerBeaconSource *_ebs;
	class EmpowerOpenAuthResponder *_eauthr;
	class EmpowerAssociationResponder *_eassor;
	class EmpowerPowerSaveBuffer *_epsb;

	LVAP _lvaps;
	EtherAddress _hwaddr;
	EtherAddress _mask;
	Timer _timer;
	String _debugfs_string;

	uint32_t _seq;
	uint32_t _port;

	unsigned int _period; // msecs
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
