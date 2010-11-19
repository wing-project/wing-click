/*
 * deaggregaotr.click
 * 
 * Sandbox for testing the packet concatenation elements
 * 
 */

FromDump('aggregator-packed.dump', STOP true)
 -> DeAggregator(ETHTYPE 0x0642)
 -> ToDump('aggregator-output.dump', ENCAP ETHER)

