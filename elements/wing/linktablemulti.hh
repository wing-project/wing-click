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
    //void dijkstra();

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

    uint32_t _beta;

    static String read_handler(Element *, void *);

    uint32_t compute_metric(Vector<uint32_t>, Vector<uint32_t>);

};

CLICK_ENDDECLS
#endif 
