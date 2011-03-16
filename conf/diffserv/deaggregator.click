/*
 * deaggregaotr.click
 * 
 * Sandbox for testing the packet concatenation elements
 * 
 */

FromDump('aggregator-packed.dump', STOP true)
 -> DeAggregator()
 -> ToDump('aggregator-output.dump', ENCAP ETHER)

