#ifndef CLICK_SR2COUNTERMULTI_HH
#define CLICK_SR2COUNTERMULTI_HH
#include <click/element.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

/*
=c

Counter([I<keywords COUNT_CALL, BYTE_COUNT_CALL>])

=s counters

measures packet count and rate

=d

Passes packets unchanged from its input to its output, maintaining statistics
information about packet count and packet rate.

Keyword arguments are:

=over 8

=item COUNT_CALL

Argument is `I<N> I<HANDLER> [I<VALUE>]'. When the packet count reaches I<N>,
call the write handler I<HANDLER> with value I<VALUE> before emitting the
packet.

=item BYTE_COUNT_CALL

Argument is `I<N> I<HANDLER> [I<VALUE>]'. When the byte count reaches or
exceeds I<N>, call the write handler I<HANDLER> with value I<VALUE> before
emitting the packet.

=back

=h count read-only

Returns the number of packets that have passed through since the last reset.

=h byte_count read-only

Returns the number of bytes that have passed through since the last reset.

=h rate read-only

Returns the recent arrival rate, measured by exponential
weighted moving average, in packets per second.

=h bit_rate read-only

Returns the recent arrival rate, measured by exponential
weighted moving average, in bits per second.

=h byte_rate read-only

Returns the recent arrival rate, measured by exponential
weighted moving average, in bytes per second.

=h reset_counts write-only

Resets the counts and rates to zero.

=h reset write-only

Same as 'reset_counts'.

=h count_call write-only

Writes a new COUNT_CALL argument. The handler can be omitted.

=h byte_count_call write-only

Writes a new BYTE_COUNT_CALL argument. The handler can be omitted.

=h CLICK_LLRPC_GET_RATE llrpc

Argument is a pointer to an integer that must be 0.  Returns the recent
arrival rate (measured by exponential weighted moving average) in
packets per second.

=h CLICK_LLRPC_GET_COUNT llrpc

Argument is a pointer to an integer that must be 0 (packet count) or 1 (byte
count). Returns the current packet or byte count.

=h CLICK_LLRPC_GET_COUNTS llrpc

Argument is a pointer to a click_llrpc_counts_st structure (see
<click/llrpc.h>). The C<keys> components must be 0 (packet count) or 1 (byte
count). Stores the corresponding counts in the corresponding C<values>
components.

*/

class SR2CounterMulti : public Element { public:

    SR2CounterMulti();
    ~SR2CounterMulti();

    const char *class_name() const		{ return "SR2CounterMulti"; }
    const char *port_count() const		{ return "1/2"; }
    const char *processing() const		{ return AGNOSTIC; }
    const char *flow_code() const		{ return "#/#"; }

    int initialize(ErrorHandler *);
    
    uint32_t read_count();
    uint32_t read_byte_count();
    uint32_t read_busy_time();
    void reset();

    void add_handlers();

    void push(int, Packet *);
    
    void set_discard(bool);

  private:

    uint32_t _count;
    uint32_t _byte_count;
		uint32_t _busy_time;
    bool _discard;

    static String read_handler(Element *, void *);
    static int write_handler(const String&, Element*, void*, ErrorHandler*);

};

CLICK_ENDDECLS
#endif
