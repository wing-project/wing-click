elementclass RateControl {
  $rate, $rates|

  rate_control :: SetTXRate(RATE $rate);

  input -> rate_control -> output;
  input [1] -> Discard();

};

