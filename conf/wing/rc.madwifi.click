elementclass RateControl {
  $rate, $rates|

  rate_control :: MadwifiRate(OFFSET 4, RT $rates, ALT_RATE true);

  input -> rate_control -> output;
  input [1] -> [1] rate_control;

};

