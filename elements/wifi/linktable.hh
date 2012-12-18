#ifndef CLICK_LINKTABLE_HH
#define CLICK_LINKTABLE_HH
#include <click/ipaddress.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include "path.hh"
CLICK_DECLS

/*
 * =c
 * LinkTable(IP Address, [STALE timeout])
 * =s Wifi
 * Keeps a Link state database and calculates Weighted Shortest Path
 * for other elements
 * =d
 * Runs dijkstra's algorithm occasionally.
 * =a ARPTable
 *
 */

template <typename T, typename U>
class LinkTableBase: public Element{
public:

  /* generic click-mandated stuff*/
  LinkTableBase();
  ~LinkTableBase();
  void add_handlers();
  const char* class_name() const { return "LinkTableBase"; }
  virtual int configure (Vector<String> &, ErrorHandler *) = 0;
  int initialize(ErrorHandler *);
  void run_timer(Timer *);
  void take_state(Element *, ErrorHandler *);
  void *cast(const char *n);
  /* read/write handlers */
  virtual String print_routes(bool, bool) = 0;
  String print_links();
  String print_hosts();
  T ip() { return _ip; }
  void clear();

  /* other public functions */
  virtual String route_to_string(U) = 0;
  bool update_link(T from, T to, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel);
  bool update_both_links(T a, T b, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel) {
    if (update_link(a,b,seq,age, metric)) {
      return update_link(b,a,seq,age, metric);
    }
    return false;
  }
  virtual U best_route(T, bool) = 0;

  virtual uint32_t get_route_metric(const U &) = 0;

  uint16_t get_link_channel(T from, T to);
  uint32_t get_link_metric(T from, T to);
  uint32_t get_link_seq(T from, T to);
  uint32_t get_link_age(T from, T to);

  bool valid_route(const U &);
  virtual void dijkstra(bool) = 0;
  void clear_stale();

  uint32_t get_host_metric_to_me(T s);
  uint32_t get_host_metric_from_me(T s);
  Vector<T> get_hosts();

  typedef HashMap<T, T> AddressTable;
  typedef typename HashMap<T, T>::const_iterator ATIter;

  HashMap<T, T> _blacklist;

protected:

  class AddressPair {
    public:
      T _to;
      T _from;
      AddressPair()
	  : _to(), _from() {
      }
      AddressPair(T from, T to)
	  : _to(to), _from(from) {
      }
      bool contains(T foo) const {
          return (foo == _to) || (foo == _from);
      }
      bool other(T foo) const {
          return (_to == foo) ? _from : _to;
      }
      inline hashcode_t hashcode() const {
          return CLICK_NAME(hashcode)(_to) + CLICK_NAME(hashcode)(_from);
      }
      inline bool operator==(AddressPair other) const {
          return (other._to == _to && other._from == _from);
      }
  };

  class LinkInfo {
  public:
    T _from;
    T _to;
    uint32_t _metric;
    uint32_t _seq;
    uint32_t _age;
    uint32_t _channel;
    Timestamp _last_updated;
    LinkInfo() {
      _from = T();
      _to = T();
      _metric = 0;
      _seq = 0;
      _age = 0;
    }

    LinkInfo(T from, T to,
	     uint32_t seq, uint32_t age, uint32_t metric, uint32_t channel) {
      _from = from;
      _to = to;
      _metric = metric;
      _seq = seq;
      _age = age;
      _channel = channel;
      _last_updated.assign_now();
    }

    LinkInfo(const LinkInfo &p) :
      _from(p._from), _to(p._to),
      _metric(p._metric), _seq(p._seq),
      _age(p._age), _channel(p._channel),
      _last_updated(p._last_updated)
    { }

    uint32_t age() {
	Timestamp now = Timestamp::now();
	return _age + (now.sec() - _last_updated.sec());
    }
    void update(uint32_t seq, uint32_t age, uint32_t metric, uint32_t channel) {
      if (seq <= _seq) {
	return;
      }
      _metric = metric;
      _seq = seq;
      _age = age;
      _channel = channel;
      _last_updated.assign_now();
    }
  };

  class HostInfo {
  public:
    T _address;
    uint32_t _metric_from_me;
    uint32_t _metric_to_me;

    T _prev_from_me;
    T _prev_to_me;

    bool _marked_from_me;
    bool _marked_to_me;

    HostInfo() {
      _address = T();
      _metric_from_me = 0;
      _metric_to_me = 0;
      _prev_from_me = T();
      _prev_to_me = T();
      _marked_from_me = false;
      _marked_to_me = false;
    }

    HostInfo(T address) {
      _address = address;
      _metric_from_me = 0;
      _metric_to_me = 0;
      _prev_from_me = T();
      _prev_to_me = T();
      _marked_from_me = false;
      _marked_to_me = false;
    }

    HostInfo(const HostInfo &p) :
      _address(p._address),
      _metric_from_me(p._metric_from_me),
      _metric_to_me(p._metric_to_me),
      _prev_from_me(p._prev_from_me),
      _prev_to_me(p._prev_to_me),
      _marked_from_me(p._marked_from_me),
      _marked_to_me(p._marked_to_me)
    { }

    void clear(bool from_me) {
      if (from_me ) {
	_prev_from_me = T();
	_metric_from_me = 0;
	_marked_from_me = false;
      } else {
	_prev_to_me = T();
	_metric_to_me = 0;
	_marked_to_me = false;
      }
    }
  };

  typedef HashMap<T, HostInfo> HTable;
  typedef typename HTable::const_iterator HTIter;

  typedef HashMap<AddressPair, LinkInfo> LTable;
  typedef typename LTable::const_iterator LTIter;

  HTable _hosts;
  LTable _links;

  T _ip;
  Timestamp _stale_timeout;
  Timer _timer;
  Timestamp _dijkstra_time;

  static int write_handler(const String &, Element *, void *, ErrorHandler *);
  static String read_handler(Element *, void *);

};

template <typename T, typename U>
LinkTableBase<T,U>::LinkTableBase()
  : _timer(this)
{
}

template <typename T, typename U>
LinkTableBase<T,U>::~LinkTableBase()
{
}

template <typename T, typename U>
int
LinkTableBase<T,U>::initialize (ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}

template <typename T, typename U>
void *
LinkTableBase<T,U>::cast(const char *n)
{
  if (strcmp(n, "LinkTableBase") == 0)
    return (LinkTableBase *) this;
  else
    return 0;
}

template <typename T, typename U>
void
LinkTableBase<T,U>::run_timer(Timer *)
{
  clear_stale();
  dijkstra(true);
  dijkstra(false);
  _timer.schedule_after_msec(5000);
}

template <typename T, typename U>
void
LinkTableBase<T,U>::take_state(Element *e, ErrorHandler *) {
  LinkTableBase *q = (LinkTableBase *)e->cast("LinkTableBase");
  if (!q) return;

  _hosts = q->_hosts;
  _links = q->_links;
  dijkstra(true);
  dijkstra(false);
}

template <typename T, typename U>
void
LinkTableBase<T,U>::clear()
{
  _hosts.clear();
  _links.clear();

}

template <typename T, typename U>
bool
LinkTableBase<T,U>::update_link(T from, T to, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel)
{
  if (!from || !to || !metric) {
    return false;
  }
  if (_stale_timeout.sec() < (int) age) {
    return true;
  }

  /* make sure both the hosts exist */
  HostInfo *nfrom = _hosts.findp(from);
  if (!nfrom) {
    _hosts.insert(from, HostInfo(from));
    nfrom = _hosts.findp(from);
  }
  HostInfo *nto = _hosts.findp(to);
  if (!nto) {
    _hosts.insert(to, HostInfo(to));
    nto = _hosts.findp(to);
  }

  assert(nfrom);
  assert(nto);

  AddressPair p = AddressPair(from, to);
  LinkInfo *lnfo = _links.findp(p);
  if (!lnfo) {
    _links.insert(p, LinkInfo(from, to, seq, age, metric, channel));
  } else {
    lnfo->update(seq, age, metric, channel);
  }
  return true;
}

template <typename T, typename U>
Vector<T>
LinkTableBase<T,U>::get_hosts()
{
  Vector<T> addrs;
  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    addrs.push_back(iter.key());
  }
  return addrs;
}

template <typename T, typename U>
uint32_t
LinkTableBase<T,U>::get_host_metric_to_me(T s)
{
  if (!s) {
    return 0;
  }
  HostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric_to_me;
}

template <typename T, typename U>
uint32_t
LinkTableBase<T,U>::get_host_metric_from_me(T s)
{
  if (!s) {
    return 0;
  }
  HostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric_from_me;
}

template <typename T, typename U>
uint16_t
LinkTableBase<T,U>::get_link_channel(T from, T to)
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  AddressPair p = AddressPair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->_channel;
}

template <typename T, typename U>
uint32_t
LinkTableBase<T,U>::get_link_metric(T from, T to)
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  AddressPair p = AddressPair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric;
}

template <typename T, typename U>
uint32_t
LinkTableBase<T,U>::get_link_seq(T from, T to)
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  AddressPair p = AddressPair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->_seq;
}

template <typename T, typename U>
uint32_t
LinkTableBase<T,U>::get_link_age(T from, T to)
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  AddressPair p = AddressPair(from, to);
  LinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->age();
}

template <typename T, typename U>
bool
LinkTableBase<T,U>::valid_route(const U &route)
{
  if (route.size() < 1) {
    return false;
  }
  /* ensure the metrics are all valid */
  unsigned metric = get_route_metric(route);
  if (metric  == 0 ||
      metric >= 999999){
    return false;
  }

  /* ensure that a node appears no more than once */
  for (int x = 0; x < route.size(); x++) {
    for (int y = x + 1; y < route.size(); y++) {
      if (route[x] == route[y]) {
        return false;
      }
    }
  }

  return true;
}

template <typename T, typename U>
String
LinkTableBase<T,U>::print_links()
{
  StringAccum sa;
  for (LTIter iter = _links.begin(); iter.live(); iter++) {
    LinkInfo n = iter.value();
    sa << n._from.unparse() << " " << n._to.unparse();
    sa << " " << n._metric;
    sa << " " << n._channel;
    sa << " " << n._seq << " " << n.age() << "\n";
  }
  return sa.take_string();
}

template <typename T>
static int addr_sorter(const void *va, const void *vb, void *) {
    T *a = (T *)va, *b = (T *)vb;
    if (a->addr() == b->addr()) {
	return 0;
    }
    return (ntohl(a->addr()) < ntohl(b->addr())) ? -1 : 1;
}

template <typename T, typename U>
String
LinkTableBase<T,U>::print_hosts()
{
  StringAccum sa;
  Vector<T> addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    addrs.push_back(iter.key());
  }

  click_qsort(addrs.begin(), addrs.size(), sizeof(T), addr_sorter<T>);

  for (int x = 0; x < addrs.size(); x++) {
    sa << addrs[x] << "\n";
  }

  return sa.take_string();
}

template <typename T, typename U>
void
LinkTableBase<T,U>::clear_stale() {
  LTable links;
  for (LTIter iter = _links.begin(); iter.live(); iter++) {
    LinkInfo nfo = iter.value();
    if ((unsigned) _stale_timeout.sec() >= nfo.age()) {
      links.insert(AddressPair(nfo._from, nfo._to), nfo);
    }
  }
  _links.clear();
  for (LTIter iter = links.begin(); iter.live(); iter++) {
    LinkInfo nfo = iter.value();
    _links.insert(AddressPair(nfo._from, nfo._to), nfo);
  }
}

enum {H_HOST_IP,
      H_BLACKLIST,
      H_BLACKLIST_CLEAR,
      H_BLACKLIST_ADD,
      H_BLACKLIST_REMOVE,
      H_LINKS,
      H_ROUTES_OLD,
      H_ROUTES_FROM,
      H_ROUTES_TO,
      H_HOSTS,
      H_CLEAR,
      H_DIJKSTRA,
      H_UPDATE_LINK,
      H_DIJKSTRA_TIME};

template <typename T, typename U>
String 
LinkTableBase<T,U>::read_handler(Element *e, void *thunk) {
  LinkTableBase *td = (LinkTableBase *) e;
  switch ((uintptr_t) thunk) {
    case H_HOST_IP:  return td->ip().unparse() + "\n";
    case H_BLACKLIST: {
      StringAccum sa;
      for (ATIter iter = td->_blacklist.begin(); iter.live(); iter++) {
	sa << iter.value() << " ";
      }
      return sa.take_string() + "\n";
    }
    case H_LINKS:  return td->print_links();
    case H_ROUTES_TO: return td->print_routes(false, true);
    case H_ROUTES_FROM: return td->print_routes(true, true);
    case H_ROUTES_OLD: return td->print_routes(true, false);
    case H_HOSTS:  return td->print_hosts();
    case H_DIJKSTRA_TIME: {
      StringAccum sa;
      sa << td->_dijkstra_time << "\n";
      return sa.take_string();
    }
    default:
      return String();
    }
}

template <typename T, typename U>
int 
LinkTableBase<T,U>::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler * errh) {

  LinkTableBase *f = (LinkTableBase *) e;
  String s = cp_uncomment(in_s);
  switch ((intptr_t) vparam) {
  case H_UPDATE_LINK: {

    Vector<String> args;
    IPAddress from;
    IPAddress to;
    uint32_t seq;
    uint32_t age;
    uint32_t metric;
    uint32_t channel;
    cp_spacevec(in_s, args);

    if (args.size() != 6) {
      return errh->error("Must have six arguments: currently has %d: %s", args.size(), args[0].c_str());
    }

    if (!cp_ip_address(args[0], &from)) {
      return errh->error("Couldn't read IPAddress out of from");
    }

    if (!cp_ip_address(args[1], &to)) {
      return errh->error("Couldn't read IPAddress out of to");
    }

    if (!cp_unsigned(args[2], &metric)) {
      return errh->error("Couldn't read metric");
    }

    if (!cp_unsigned(args[3], &seq)) {
      return errh->error("Couldn't read seq");
    }

    if (!cp_unsigned(args[4], &age)) {
      return errh->error("Couldn't read age");
    }

    if (!cp_unsigned(args[5], &channel)) {
      return errh->error("Couldn't read channel");
    }

    f->update_link(from, to, seq, age, metric, channel);
    break;

  }
  case H_BLACKLIST_CLEAR: {
    f->_blacklist.clear();
    break;
  }
  case H_BLACKLIST_ADD: {
    IPAddress m;
    if (!cp_ip_address(s, &m))
      return errh->error("blacklist_add parameter must be ipaddress");
    f->_blacklist.insert(m, m);
    break;
  }
  case H_BLACKLIST_REMOVE: {
    IPAddress m;
    if (!cp_ip_address(s, &m))
      return errh->error("blacklist_add parameter must be ipaddress");
    f->_blacklist.erase(m);
    break;
  }
  case H_CLEAR: f->clear(); break;
  case H_DIJKSTRA: f->dijkstra(true); f->dijkstra(false); break;
  }
  return 0;
}

template <typename T, typename U>
void
LinkTableBase<T,U>::add_handlers() {
  add_read_handler("ip", read_handler, H_HOST_IP);
  add_read_handler("routes", read_handler, H_ROUTES_FROM);
  add_read_handler("routes_old", read_handler, H_ROUTES_OLD);
  add_read_handler("routes_from", read_handler, H_ROUTES_FROM);
  add_read_handler("routes_to", read_handler, H_ROUTES_TO);
  add_read_handler("links", read_handler, H_LINKS);
  add_read_handler("hosts", read_handler, H_HOSTS);
  add_read_handler("blacklist", read_handler, H_BLACKLIST);
  add_read_handler("dijkstra_time", read_handler, H_DIJKSTRA_TIME);
  add_write_handler("clear", write_handler, H_CLEAR);
  add_write_handler("blacklist_clear", write_handler, H_BLACKLIST_CLEAR);
  add_write_handler("blacklist_add", write_handler, H_BLACKLIST_ADD);
  add_write_handler("blacklist_remove", write_handler, H_BLACKLIST_REMOVE);
  add_write_handler("dijkstra", write_handler, H_DIJKSTRA);
  add_write_handler("update_link", write_handler, H_UPDATE_LINK);
}

class LinkTable : public LinkTableBase<IPAddress, Path> { public:

    LinkTable();
    ~LinkTable();

    const char *class_name() const		{ return "LinkTable"; }
    int configure (Vector<String> &, ErrorHandler *);

    Path best_route(IPAddress, bool);
    String print_routes(bool, bool);
    String route_to_string(Path);
    uint32_t get_route_metric(const Path &);
    void dijkstra(bool);

};

CLICK_ENDDECLS
#endif /* CLICK_LINKTABLE_HH */
