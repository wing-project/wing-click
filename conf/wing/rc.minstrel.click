elementclass RateControl {
  $rate, $rates, __REST__ $rates_ht|                    
                                                
  rate_control :: Minstrel(4, $rates, $rates_ht);

  input -> rate_control -> output;
  input [1] -> [1] rate_control [1] -> [1] output;

};

