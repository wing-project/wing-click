elementclass LinkScheduler {
  $lt, $arp|

  input -> FairBuffer(SCHEDULER true, AGGREGATOR false) -> output;
  
};

