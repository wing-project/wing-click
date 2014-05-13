#ifndef CLICK_ARPTABLE_HH
#define CLICK_ARPTABLE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include <click/config.h>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

ARPTable(I<keywords>)

=s arp

stores IP-to-Ethernet mappings

=d

The ARPTable element stores IP-to-Ethernet mappings, such as are useful for
the ARP protocol.  ARPTable is an information element, with no inputs or
outputs.  ARPQuerier normally encapsulates access to an ARPTable element.  A
separate ARPTable is useful if several ARPQuerier elements should share a
table.

Keyword arguments are:

=over 8

=item CAPACITY

Unsigned integer.  The maximum number of saved IP packets the ARPTable will
hold at a time.  Default is 2048; zero means unlimited.

=item ENTRY_CAPACITY

Unsigned integer.  The maximum number of ARP entries the ARPTable will hold at
a time.  Default is zero, which means unlimited.

=item ENTRY_PACKET_CAPACITY

Unsigned integer.  The maximum number of saved IP packets the ARPTable will hold
for any given ARP entry at a time.  Default is zero, which means unlimited.

=item CAPACITY_SLIM_FACTOR

Unsigned integer. ARPTable removes 1/CAPACITY_SLIM_FACTOR of saved packets on
exceeding CAPACITY. Default is 2.  Increase for better bounds on the number of
saved packets; decrease for better performance under an nmap-style denial-of-
service attack.

=item TIMEOUT

Time value.  The amount of time after which an ARP entry will expire.  Default
is 5 minutes.  Zero means ARP entries never expire.

=h table r

Return a table of the ARP entries.  The returned string has four
space-separated columns: an IP address, whether the entry is valid (1 means
valid, 0 means not), the corresponding Ethernet address, and finally, the
amount of time since the entry was last updated.

=h drops r

Return the number of packets dropped because of timeouts or capacity limits.

=h insert w

Add an entry to the table.  The format should be "IP ETH".

=h delete w

Delete an entry from the table.  The string should consist of an IP address.

=h clear w

Clear the table, deleting all entries.

=h count r

Return the number of entries in the table.

=h length r

Return the number of packets stored in the table.

=a

ARPQuerier
*/

template <typename T> 
class ARPTableBase : public Element { public:

    ARPTableBase() CLICK_COLD;
    ~ARPTableBase() CLICK_COLD;

    const char *class_name() const		{ return "ARPTableBase"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const		{ return true; }
    void take_state(Element *, ErrorHandler *);
    void add_handlers() CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    int lookup(T ip, EtherAddress *eth, uint32_t poll_timeout_j);
    EtherAddress lookup(T ip);
    T reverse_lookup(const EtherAddress &eth);
    int insert(T ip, const EtherAddress &en, Packet **head = 0);
    int append_query(T ip, Packet *p);
    void clear();

    uint32_t capacity() const {
	return _packet_capacity;
    }
    void set_capacity(uint32_t capacity) {
	_packet_capacity = capacity;
    }
    uint32_t entry_capacity() const {
	return _entry_capacity;
    }
    void set_entry_capacity(uint32_t entry_capacity) {
	_entry_capacity = entry_capacity;
    }
    uint32_t entry_packet_capacity() const {
	return _entry_packet_capacity;
    }
    void set_entry_packet_capacity(uint32_t entry_packet_capacity) {
	_entry_packet_capacity = entry_packet_capacity;
    }
    uint32_t capacity_slim_factor() const {
	return _capacity_slim_factor;
    }
    void set_capacity_slim_factor(uint32_t capacity_slim_factor) {
        assert(capacity_slim_factor != 0);
	_capacity_slim_factor = capacity_slim_factor;
    }
    Timestamp timeout() const {
	return Timestamp::make_jiffies((click_jiffies_t) _timeout_j);
    }
    void set_timeout(const Timestamp &timeout) {
	if ((uint32_t) timeout.sec() >= (uint32_t) 0xFFFFFFFFU / CLICK_HZ)
	    _timeout_j = 0;
	else
	    _timeout_j = timeout.jiffies();
    }

    uint32_t drops() const {
	return _drops;
    }
    uint32_t count() const {
	return _entry_count;
    }
    uint32_t length() const {
	return _packet_count;
    }

    void run_timer(Timer *);

    enum {
	h_table, h_insert, h_delete, h_clear
    };
    static String read_handler(Element *e, void *user_data) CLICK_COLD;
    static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;

    struct ARPEntry {		// This structure is now larger than I'd like
	T _ip;		// (40B) but probably still fine.
	ARPEntry *_hashnext;
	EtherAddress _eth;
	bool _known;
	uint8_t _num_polls_since_reply;
	click_jiffies_t _live_at_j;
	click_jiffies_t _polled_at_j;
	Packet *_head;
	Packet *_tail;
	uint32_t _entry_packet_count;
	List_member<ARPEntry> _age_link;
	typedef T key_type;
	typedef T key_const_reference;
	ARPEntry(T ip)
	    : _ip(ip), _hashnext(), _eth(EtherAddress::make_broadcast()),
	      _known(false), _num_polls_since_reply(0), _head(), _tail(), _entry_packet_count(0) {
	}
	key_const_reference hashkey() const {
	    return _ip;
	}
	bool expired(click_jiffies_t now, uint32_t timeout_j) const {
	    return click_jiffies_less(_live_at_j + timeout_j, now)
		&& timeout_j;
	}
	bool known(click_jiffies_t now, uint32_t timeout_j) const {
	    return _known && !expired(now, timeout_j);
	}
	bool allow_poll(click_jiffies_t now) const {
	    click_jiffies_t thresh_j = _polled_at_j
		+ (_num_polls_since_reply >= 10 ? CLICK_HZ * 2 : CLICK_HZ / 10);
	    return !click_jiffies_less(now, thresh_j);
	}
	void mark_poll(click_jiffies_t now) {
	    _polled_at_j = now;
	    if (_num_polls_since_reply < 255)
		++_num_polls_since_reply;
	}
    };

  protected:

    ReadWriteLock _lock;

    typedef HashContainer<ARPEntry> Table;
    typedef typename Table::iterator TIter;

    Table _table;
    typedef List<ARPEntry, &ARPEntry::_age_link> AgeList;
    AgeList _age;
    atomic_uint32_t _entry_count;
    atomic_uint32_t _packet_count;
    uint32_t _entry_capacity;
    uint32_t _packet_capacity;
    uint32_t _entry_packet_capacity;
    uint32_t _capacity_slim_factor;
    uint32_t _timeout_j;
    atomic_uint32_t _drops;
    SizedHashAllocator<sizeof(ARPEntry)> _alloc;
    Timer _expire_timer;

    ARPEntry *ensure(T ip, click_jiffies_t now);
    void slim(click_jiffies_t now);

};

template <typename T>
inline int 
ARPTableBase<T>::lookup(T ip, EtherAddress *eth, uint32_t poll_timeout_j)
{
    _lock.acquire_read();
    int r = -1;
    if (TIter it = _table.find(ip)) {
	click_jiffies_t now = click_jiffies();
	if (it->known(now, _timeout_j)) {
	    *eth = it->_eth;
	    if (poll_timeout_j
		&& !click_jiffies_less(now, it->_live_at_j + poll_timeout_j)
		&& it->allow_poll(now)) {
		it->mark_poll(now);
		r = 1;
	    } else
		r = 0;
	}
    }
    _lock.release_read();
    return r;
}

template <typename T>
inline EtherAddress
ARPTableBase<T>::lookup(T ip)
{
    EtherAddress eth;
    if (lookup(ip, &eth, 0) >= 0)
	return eth;
    else
	return EtherAddress::make_broadcast();
}

template <typename T>
ARPTableBase<T>::ARPTableBase()
    : _entry_capacity(0), _packet_capacity(2048), _expire_timer(this)
{
    _entry_count = _packet_count = _drops = 0;
}

template <typename T>
ARPTableBase<T>::~ARPTableBase()
{
}

template <typename T>
int
ARPTableBase<T>::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Timestamp timeout(300);
    if (Args(conf, this, errh)
	  .read("CAPACITY", _packet_capacity)
	  .read("ENTRY_CAPACITY", _entry_capacity)
	  .read("TIMEOUT", timeout)
	  .complete() < 0)
	return -1;
    set_timeout(timeout);
    if (_timeout_j) {
	_expire_timer.initialize(this);
	_expire_timer.schedule_after_sec(_timeout_j / CLICK_HZ);
    }
    return 0;
}

template <typename T>
void
ARPTableBase<T>::cleanup(CleanupStage)
{
    clear();
}

template <typename T>
void
ARPTableBase<T>::clear()
{
    // Walk the arp cache table and free any stored packets and arp entries.
    for (TIter it = _table.begin(); it; ) {
	ARPEntry *ae = _table.erase(it);
	while (Packet *p = ae->_head) {
	    ae->_head = p->next();
	    p->kill();
	    ++_drops;
	}
	_alloc.deallocate(ae);
    }
    _entry_count = _packet_count = 0;
    _age.__clear();
}

template <typename T>
void
ARPTableBase<T>::take_state(Element *e, ErrorHandler *errh)
{
    ARPTableBase<T> *arpt = (ARPTableBase<T> *)e->cast("ARPTableBase");
    if (!arpt)
	return;
    if (_table.size() > 0) {
	errh->error("late take_state");
	return;
    }

    _table.swap(arpt->_table);
    _age.swap(arpt->_age);
    _entry_count = arpt->_entry_count;
    _packet_count = arpt->_packet_count;
    _drops = arpt->_drops;
    _alloc.swap(arpt->_alloc);

    arpt->_entry_count = 0;
    arpt->_packet_count = 0;
}

template <typename T>
void
ARPTableBase<T>::slim(click_jiffies_t now)
{
    ARPEntry *ae;

    // Delete old entries.
    while ((ae = _age.front())
	   && (ae->expired(now, _timeout_j)
	       || (_entry_capacity && _entry_count > _entry_capacity))) {
	_table.erase(ae->_ip);
	_age.pop_front();

	while (Packet *p = ae->_head) {
	    ae->_head = p->next();
	    p->kill();
	    --_packet_count;
	    ++_drops;
	}

	_alloc.deallocate(ae);
	--_entry_count;
    }

    // Mark entries for polling, and delete packets to make space.
    while (_packet_capacity && _packet_count > _packet_capacity) {
	while (ae->_head && _packet_count > _packet_capacity) {
	    Packet *p = ae->_head;
	    if (!(ae->_head = p->next()))
		ae->_tail = 0;
	    p->kill();
	    --_packet_count;
	    ++_drops;
	}
	ae = ae->_age_link.next();
    }
}

template <typename T>
void
ARPTableBase<T>::run_timer(Timer *timer)
{
    // Expire any old entries, and make sure there's room for at least one
    // packet.
    _lock.acquire_write();
    slim(click_jiffies());
    _lock.release_write();
    if (_timeout_j)
	timer->schedule_after_sec(_timeout_j / CLICK_HZ + 1);
}

template <typename T>
typename ARPTableBase<T>::ARPEntry* 
ARPTableBase<T>::ensure(T ip, click_jiffies_t now)
{
    _lock.acquire_write();
    TIter it = _table.find(ip);
    if (!it) {
	void *x = _alloc.allocate();
	if (!x) {
	    _lock.release_write();
	    return 0;
	}

	++_entry_count;
	if (_entry_capacity && _entry_count > _entry_capacity)
	    slim(now);

	ARPEntry *ae = new(x) ARPEntry(ip);
	ae->_live_at_j = now;
	ae->_polled_at_j = ae->_live_at_j - CLICK_HZ;
	_table.set(it, ae);

	_age.push_back(ae);
    }
    return it.get();
}

template <typename T>
int
ARPTableBase<T>::insert(T ip, const EtherAddress &eth, Packet **head)
{
    click_jiffies_t now = click_jiffies();
    ARPEntry *ae = ensure(ip, now);
    if (!ae)
	return -ENOMEM;

    ae->_eth = eth;
    ae->_known = !eth.is_broadcast();

    ae->_live_at_j = now;
    ae->_polled_at_j = ae->_live_at_j - CLICK_HZ;

    if (ae->_age_link.next()) {
	_age.erase(ae);
	_age.push_back(ae);
    }

    if (head) {
	*head = ae->_head;
	ae->_head = ae->_tail = 0;
	for (Packet *p = *head; p; p = p->next())
	    --_packet_count;
    }

    _table.balance();
    _lock.release_write();
    return 0;
}

template <typename T>
int
ARPTableBase<T>::append_query(T ip, Packet *p)
{
    click_jiffies_t now = click_jiffies();
    ARPEntry *ae = ensure(ip, now);
    if (!ae)
	return -ENOMEM;

    if (ae->known(now, _timeout_j)) {
	_lock.release_write();
	return -EAGAIN;
    }

    // Since we're still trying to send to this address, keep the entry just
    // this side of expiring.  This fixes a bug reported 5 Nov 2009 by Seiichi
    // Tetsukawa, and verified via testie, where the slim() below could delete
    // the "ae" ARPEntry when "ae" was the oldest entry in the system.
    if (_timeout_j) {
	click_jiffies_t live_at_j_min = now - _timeout_j;
	if (click_jiffies_less(ae->_live_at_j, live_at_j_min)) {
	    ae->_live_at_j = live_at_j_min;
	    // Now move "ae" to the right position in the list by walking
	    // forward over other elements (potentially expensive?).
	    ARPEntry *ae_next = ae->_age_link.next(), *next = ae_next;
	    while (next && click_jiffies_less(next->_live_at_j, ae->_live_at_j))
		next = next->_age_link.next();
	    if (ae_next != next) {
		_age.erase(ae);
		_age.insert(next /* might be null */, ae);
	    }
	}
    }

    ++_packet_count;
    if (_packet_capacity && _packet_count > _packet_capacity)
	slim(now);

    if (ae->_tail)
	ae->_tail->set_next(p);
    else
	ae->_head = p;
    ae->_tail = p;
    p->set_next(0);

    int r;
    if (ae->allow_poll(now)) {
        ae->mark_poll(now);
        r = 1;
    } else
        r = 0;

    _table.balance();
    _lock.release_write();
    return r;
}

template <typename T>
T
ARPTableBase<T>::reverse_lookup(const EtherAddress &eth)
{
    _lock.acquire_read();

    T ip;
    for (TIter it = _table.begin(); it; ++it)
	if (it->_eth == eth) {
	    ip = it->_ip;
	    break;
	}

    _lock.release_read();
    return ip;
}

template <typename T>
String
ARPTableBase<T>::read_handler(Element *e, void *user_data)
{
    ARPTableBase *arpt = (ARPTableBase *) e;
    StringAccum sa;
    click_jiffies_t now = click_jiffies();
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_table:
	for (ARPEntry *ae = arpt->_age.front(); ae; ae = ae->_age_link.next()) {
	    int ok = ae->known(now, arpt->_timeout_j);
	    sa << ae->_ip << ' ' << ok << ' ' << ae->_eth << ' '
	       << Timestamp::make_jiffies(now - ae->_live_at_j) << '\n';
	}
	break;
    }
    return sa.take_string();
}

template <typename T>
int
ARPTableBase<T>::write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh)
{
    ARPTableBase<T> *arpt = (ARPTableBase<T> *) e;
    switch (reinterpret_cast<uintptr_t>(user_data)) {
      case h_insert: {
	  IPAddress ip;
	  EtherAddress eth;
	  if (Args(arpt, errh).push_back_words(str)
	        .read_mp("IP", ip)
	        .read_mp("ETH", eth)
	        .complete() < 0)
	      return -1;
	  arpt->insert(ip, eth);
	  return 0;
      }
      case h_delete: {
	  IPAddress ip;
	  if (Args(arpt, errh).push_back_words(str)
	        .read_mp("IP", ip)
	        .complete() < 0)
	      return -1;
	  arpt->insert(ip, EtherAddress::make_broadcast()); // XXX?
	  return 0;
      }
      case h_clear:
	arpt->clear();
	return 0;
      default:
	return -1;
    }
}

template <typename T>
void
ARPTableBase<T>::add_handlers()
{
    add_read_handler("table", read_handler, h_table);
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    add_data_handlers("count", Handler::OP_READ, &_entry_count);
    add_data_handlers("length", Handler::OP_READ, &_packet_count);
    add_write_handler("insert", write_handler, h_insert);
    add_write_handler("delete", write_handler, h_delete);
    add_write_handler("clear", write_handler, h_clear);
}

class ARPTable : public ARPTableBase<IPAddress> { public:

    ARPTable();
    ~ARPTable();

    const char *class_name() const		{ return "ARPTable"; }

};

CLICK_ENDDECLS
#endif
