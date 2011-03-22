elementclass LinkScheduler {
  $lt, $arp|

  input -> FairBuffer(LT $lt, ARP $arp, QUANTUM 12000) -> output;
  
};

