/*
 * fairbuffer.click
 * 
 * Sandbox for testing the fairbuffer scheduling elements
 * 
 */

RatedSource(DATASIZE 1470, RATE 1000, LIMIT 10000, STOP true)
 -> UDPIPEncap(1.0.0.1, 1234, 2.0.0.1, 1234) 
 -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 -> FairBuffer()
 -> RatedUnqueue(500)
 -> Discard();

