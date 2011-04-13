#ifndef CLICK_ARPTABLEMULTI_HH
#define CLICK_ARPTABLEMULTI_HH
#include <elements/ethernet/arptable.hh>
#include "wingpacket.hh"
CLICK_DECLS

class ARPTableMulti : public ARPTableBase<NodeAddress> { public:

    ARPTableMulti();
    ~ARPTableMulti();

    const char *class_name() const		{ return "ARPTableMulti"; }

};

CLICK_ENDDECLS
#endif
