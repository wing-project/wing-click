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

typedef HashMap<uint16_t, uint32_t> MetricTable;
typedef HashMap<uint16_t, uint32_t>::const_iterator MTIter;

class LinkTableMulti : public LinkTableBase<NodeAddress, PathMulti> { 

  public:

    LinkTableMulti();
    ~LinkTableMulti();

    const char *class_name() const { return "LinkTableMulti"; }
    void *cast(const char *);
    int configure (Vector<String> &, ErrorHandler *);

    PathMulti best_route(IPAddress, bool);
    String print_routes(bool, bool);
    String route_to_string(PathMulti);
    uint32_t get_route_metric(const PathMulti &);

    void dijkstra(bool);

    inline Vector<int> get_local_interfaces() { 
        HostInfoMulti *nfo = _hosts.findp(_ip);
        if (nfo) {
            return nfo->_interfaces;
        }
        return Vector<int>();
    }

  protected:

    class HostInfoMulti : public HostInfo {
      public:
	uint8_t _if_from_me;
	uint8_t _if_to_me;
        MetricTable _metric_table_from_me;
	MetricTable _metric_table_to_me;
        Vector<int> _interfaces;
	void add_interface(uint8_t iface) {
		if (iface == 0) {
			return;
		}
		for (Vector<int>::iterator iter = _interfaces.begin(); iter != _interfaces.end(); iter ++){
			if (iface == *iter) {
				return;
			}
		}
		_interfaces.push_back(iface);
	}
    };

    typedef HashMap<IPAddress, HostInfoMulti> HTable;
    typedef HTable::const_iterator HTIter;

    HTable _hosts;

    bool _wcett;
    uint32_t _beta;

};

CLICK_ENDDECLS
#endif 
