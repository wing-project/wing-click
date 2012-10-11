elementclass RateControl {
  $rate, $rates, __REST__ $rates_ht|  

  rate_control :: SetTXRate(RATE $rate);

  input -> rate_control -> output;
  input [1] -> [1] output;

};

