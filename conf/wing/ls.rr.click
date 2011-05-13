elementclass LinkScheduler {
  $lt, $arp|

  classifier :: IPClassifier(ip dscp 34, ip dscp 18, -);
  input -> classifier;

  sched :: RoundRobinSched()
    -> output;

  classifier[0] -> FullNoteQueue() -> [0] sched;
  classifier[1] -> FullNoteQueue() -> [1] sched;
  classifier[2] -> FullNoteQueue() -> [2] sched;

}

