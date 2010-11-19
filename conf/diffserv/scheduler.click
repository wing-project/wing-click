/*
 * scheduler.click
 * 
 * Sandbox for testing scheduling elements 
 * 
 */

scheduler :: WFQSched (8,4,2,1)
 -> BandwidthRatedUnqueue(1000000 Bps) 
 -> split :: IPClassifier(ip dscp 26, ip dscp 18, ip dscp 10, -);

InfiniteSource(DATASIZE 750)
 -> UDPIPEncap(127.0.0.1, 44444, 127.0.0.1, 4010)
 -> EtherEncap(0x0800, 00:50:BA:85:84:A9, 00:50:BA:85:84:A9)
 -> SetIPDSCP(26)
 -> Queue(500)
 -> [0] scheduler;

InfiniteSource(DATASIZE 1500)
 -> UDPIPEncap(127.0.0.1, 44444, 127.0.0.1, 4010)
 -> EtherEncap(0x0800, 00:50:BA:85:84:A9, 00:50:BA:85:84:A9)
 -> SetIPDSCP(18)
 -> Queue(500)
 -> [1] scheduler;

InfiniteSource(DATASIZE 1500)
 -> UDPIPEncap(127.0.0.1, 44444, 127.0.0.1, 4010)
 -> EtherEncap(0x0800, 00:50:BA:85:84:A9, 00:50:BA:85:84:A9)
 -> SetIPDSCP(10)
 -> Queue(500)
 -> [2] scheduler;

InfiniteSource(DATASIZE 1500)
 -> UDPIPEncap(127.0.0.1, 44444, 127.0.0.1, 4010)
 -> EtherEncap(0x0800, 00:50:BA:85:84:A9, 00:50:BA:85:84:A9)
 -> Queue(500)
 -> [3] scheduler;

split[0]
 -> hi :: AverageCounter
 -> Discard();

split[1]
 -> me :: AverageCounter
 -> Discard();

split[2]
 -> lo :: AverageCounter
 -> Discard();

split[3]
 -> be :: AverageCounter
 -> Discard();

Script(read hi.rate, wait 5, loop);
Script(read me.rate, wait 5, loop);
Script(read lo.rate, wait 5, loop);
Script(read be.rate, wait 5, loop);

