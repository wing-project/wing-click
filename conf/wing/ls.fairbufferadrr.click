elementclass LinkScheduler {
  $lt, $arp|

  input -> FairBuffer(LT $lt, ARP $arp) -> output;
  
};

