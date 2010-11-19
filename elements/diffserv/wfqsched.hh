#ifndef CLICK_WFQSCHED_HH
#define CLICK_WFQSCHED_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * WFQSched(WEIGHT<0>, ..., WEIGHT<N>)
 * =s scheduling
 * pulls from weighted round robin inputs
 * =io
 * one output, zero or more inputs
 * =d
 * Performs simple packet-based weighted round robin scheduling, 
 * assigning WEIGHT<i> to input <i> for each input.
 * 
 * The inputs usually come from Queues or other pull schedulers.
 * WFQSched uses notification to avoid pulling from empty inputs.
 *
 * =h weight0...weight<N-1> read/write
 * Returns or sets the weight for each input port.
 */
 
 
class WFQSched : public Element { public:
	
	WFQSched();
	~WFQSched();	
	
	const char *class_name() const		{ return "WFQSched"; }
	const char *port_count() const		{ return "-/1"; }
	const char *processing() const		{ return PULL; }
	
	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);
	void cleanup(CleanupStage);
    
	Packet *pull(int port);
    
private:

	NotifierSignal* _signals;

	int _next;
	int _weights_sum;

	int* _schedule;
	int* _weights;
	int* _extractions;
	int* _skips;  

	void find_schedule();

};

CLICK_ENDDECLS
#endif

