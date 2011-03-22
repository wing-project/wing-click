elementclass LinkScheduler {
  $lt, $arp|

  input -> FairBuffer(SCHEDULER true, AGGREGATOR false, LT $lt, ARP $arp, QUANTUM 12000) -> output;
  
};

