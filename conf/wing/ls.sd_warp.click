elementclass LinkScheduler {
  $lt, $arp|

  acks :: IPClassifier(tcp ack && not tcp syn && not tcp fin && ip len < 100, -);
  classifier :: IPClassifier(ip dscp 46, -);

  input -> acks;
  prio :: PrioSched()
    -> output;

  sched :: WFQSched (4,1)
    -> [1] prio;

  acks[0] -> FullNoteQueue -> [0] prio;
  acks[1] -> classifier;

  classifier[0] -> FullNoteQueue(500) -> [0] sched;
  classifier[1] -> FullNoteQueue(500) -> [1] sched;

}

