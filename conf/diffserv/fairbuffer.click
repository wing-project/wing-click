/*
 * aggregator.click
 * 
 * Sandbox for testing the packet concatenation elements
 * 
 */

RatedSource(DATASIZE 30, RATE 100, LIMIT 1000, STOP true)
 -> UDPIPEncap(1.0.0.1, 1234, 2.0.0.1, 1234) 
 -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 -> ToDump('aggregator-input.dump', ENCAP ETHER)
 -> AggregatorBuffer()
 -> RatedUnqueue(20) 
 -> ToDump('aggregator-packed.dump', ENCAP ETHER);

