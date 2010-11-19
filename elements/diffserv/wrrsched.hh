#ifndef CLICK_WRRSCHED_HH
#define CLICK_WRRSCHED_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * WRRSched(WEIGHT<0>, ..., WEIGHT<N>)
 * =s scheduling
 * pulls from weighted round robin inputs
 * =io
 * one output, zero or more inputs
 * =d
 * Performs simple packet-based weighted round robin scheduling, 
 * assigning WEIGHT<i> to input <i>. Each time a pull comes in on 
 * the output, it pulls on its input <i> with probability p which 
 * proportional to the weight assigned to the same input.
 *
 * The inputs usually come from Queues or other pull schedulers.
 * WRRSched uses notification to avoid pulling from empty inputs.
 *
 * =h weight0...weight<N-1> read/write
 * Returns or sets the weight for each input port.
 */
 
 
class WRRSched : public Element { public:
	
	WRRSched();
	~WRRSched();	
	
	const char *class_name() const		{ return "WRRSched"; }
	const char *port_count() const		{ return "-/1"; }
	const char *processing() const		{ return PULL; }
	
	int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);
	void cleanup(CleanupStage);
    
	Packet *pull(int port);

private:

	NotifierSignal* _signals;
	int* _weights;

};

CLICK_ENDDECLS
#endif

