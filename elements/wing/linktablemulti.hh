#ifndef CLICK_LINKTABLEMULTI_HH
#define CLICK_LINKTABLEMULTI_HH
#include <elements/wifi/linktable.hh>
#include <clicknet/ether.h>
#include "wingpacket.hh"
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

    inline bool update_link(NodeAddress node) {	
      if (LinkTableBase<NodeAddress, PathMulti>::update_link(node, node._ip, Timestamp::now().sec(), 0, 1, 1)) {
        return LinkTableBase<NodeAddress, PathMulti>::update_link(node._ip, node, Timestamp::now().sec(), 0, 1, 1);
      }
      return false;
    }

    inline bool update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t age, uint32_t metric, uint16_t channel) {
      if (update_link(from) &&
          update_link(to) && 
          LinkTableBase<NodeAddress, PathMulti>::update_link(from, to, seq, age, metric, channel)) {
            return true;
      }
      return false;
    }

    bool update_link_table(Packet *);

    inline Vector<int> get_local_interfaces() {
        Vector<int> ifaces;
        for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
            if ((iter.key()._ip == _ip._ip) && (iter.key()._iface != 0)) {
                ifaces.push_back(iter.key()._iface);
            }
        }
        return ifaces;
    }

  protected:

    uint32_t _beta;
    bool _debug;

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

    uint32_t compute_metric(Vector<uint32_t>, Vector<uint32_t>);

};

CLICK_ENDDECLS
#endif 
