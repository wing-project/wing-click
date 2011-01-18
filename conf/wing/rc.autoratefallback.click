elementclass RateControl {
  $rate, $rates|

  rate_control :: AutoRateFallback(OFFSET 4, RT $rates);

  input -> rate_control -> output;
  input [1] -> [1] rate_control [1] -> [1] output;

};

