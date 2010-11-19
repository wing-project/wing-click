#ifndef CLICK_DWRRSCHED_HH
#define CLICK_DWRRSCHED_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * DWRRSched(WEIGHT<0>, ..., WEIGHT<N>)
 * =s scheduling
 * pulls from inputs with weighted deficit round robin scheduling
 * =io
 * one output, zero or more inputs
 * =d
 * Schedules packets with weighted deficit round robin scheduling
 * assigning WEIGHT<i> to input <i>.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * DWRRSched uses notification to avoid pulling from empty inputs.
 *
 * =n
 * 
 * DWRRSched is a notifier signal, active if any of the upstream notifiers
 * are active.
 */

class DWRRSched : public Element { public:
  
    DWRRSched();
    ~DWRRSched();
  
    const char *class_name() const		{ return "DWRRSched"; }
    const char *port_count() const		{ return "-/1"; }
    const char *processing() const		{ return PULL; }
    void *cast(const char *);
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
  
    Packet *pull(int port);
    
  private:

    uint32_t compute_deficit(const Packet*);

    Packet **_head; // First packet from each queue.
    unsigned *_deficit;  // Each queue's deficit.

    NotifierSignal *_signals;	// upstream signals
    Notifier _notifier;
    int _next;      // Next input to consider.

    int* _quantum;
  
    class LinkTableMulti* _link_table;
    class ARPTableMulti* _arp_table;

    uint32_t _scaling;
};

CLICK_ENDDECLS
#endif
