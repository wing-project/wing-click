elementclass LinkScheduler {
  $lt, $arp|

  classifier :: IPClassifier(ip dscp 46, -);
  input -> classifier;

  sched :: RoundRobinSched()
    -> output;

  classifier[0] -> FullNoteQueue(10) -> [0] sched;
  classifier[1] -> FullNoteQueue(10) -> [1] sched;

}

