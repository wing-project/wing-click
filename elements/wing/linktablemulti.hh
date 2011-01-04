#ifndef CLICK_LINKTABLEMULTI_HH
#define CLICK_LINKTABLEMULTI_HH
#include <elements/wifi/linktable.hh>
#include "nodeaddress.hh"
#include "pathmulti.hh"
CLICK_DECLS

/*
 * =c
 * LinkTableMulti(IP Address, [STALE timeout])
 * =s Wifi
 * Keeps a Link state database and calculates Weighted Shortest Path
 * for other elements
 * =d
 * Runs dijkstra's algorithm occasionally.
 * =a ARPTable
 *
 */

class LinkTableMulti : public LinkTableBase<NodeAddress, PathMulti> { 

  public:

    LinkTableMulti();
    ~LinkTableMulti();

    const char *class_name() const { return "LinkTableMulti"; }
    void *cast(const char *);
    int configure (Vector<String> &, ErrorHandler *);
    void add_handlers();

    PathMulti best_route(NodeAddress, bool);
    String print_routes(bool, bool);
    String route_to_string(PathMulti);
    uint32_t get_route_metric(const PathMulti &);
    void dijkstra(bool);

    bool update_link(NodeAddress node) {	
      if (LinkTableBase<NodeAddress, PathMulti>::update_link(node, node._ip, Timestamp::now().sec(), 0, 1, 1)) {
        return LinkTableBase<NodeAddress, PathMulti>::update_link(node._ip, node, Timestamp::now().sec(), 0, 1, 1);
      }
      return false;
    }

    bool update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel) {
      if (update_link(from)) {
        if (update_link(to)) {
          return LinkTableBase<NodeAddress, PathMulti>::update_link(from, to, seq, age, metric, channel);
        }
      }
      return false;
    }
	
    Vector<int> get_interfaces(NodeAddress);
    Vector<int> get_local_interfaces() {
      return get_interfaces(_ip);
    }

  protected:

    typedef HashMap<uint16_t, uint32_t> MetricTable;

    class HostInfoMulti : public HostInfo {
      public:

      MetricTable _metric_table_from_me;
      MetricTable _metric_table_to_me;

      uint8_t _if_from_me;
      uint8_t _if_to_me;

      HostInfoMulti() : HostInfo(NodeAddress()), _if_from_me(0), _if_to_me(0) {
      }

      HostInfoMulti(NodeAddress address) : HostInfo(address), _if_from_me(0), _if_to_me(0) {
      }

      HostInfoMulti(const HostInfoMulti &p) : HostInfo(p), _if_from_me(p._if_from_me), _if_to_me(p._if_to_me) {
      }

    };

    typedef HashMap<NodeAddress, HostInfoMulti> HTable;
    typedef HTable::const_iterator HTIter;

    HTable _hosts;
    uint32_t _beta;

    static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif 
