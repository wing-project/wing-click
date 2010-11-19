/*
 * fairqueue.click
 * 
 * Sandbox for testing the FairQueue element
 * 
 */

data_long :: InfiniteSource(DATASIZE 1500);
data_short :: InfiniteSource(DATASIZE 1500);

sched :: RoundRobinSched
 -> Unqueue()
 -> FairBuffer()
 -> BandwidthRatedUnqueue(1000000) 
 -> cl :: Classifier(12/0647, -);

cl [0]
 -> long :: AverageCounter
 -> Discard();

cl [1]
 -> short :: AverageCounter
 -> Discard();

data_long
 -> UDPIPEncap(1.0.0.2, 1234, 2.0.0.2, 1234) 
 -> EtherEncap(0x0647, 3:3:3:3:3:3, 4:4:4:4:4:4)
 -> Queue()
 -> [0] sched;

data_short
 -> UDPIPEncap(1.0.0.1, 1234, 2.0.0.1, 1234) 
 -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
 -> Queue()
 -> [1] sched;

Script(wait 1, read long.rate, wait 5, loop);
Script(wait 1, read short.rate, wait 5, loop);

