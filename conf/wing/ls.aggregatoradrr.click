elementclass LinkScheduler {
  $lt, $arp|

  input -> AggregatorBuffer(ETHTYPE 0x0642, LT $lt, ARP $arp) -> output;
  
};

