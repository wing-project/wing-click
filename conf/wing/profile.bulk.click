elementclass WingRouter {
  $ip, $nm, $bcast, $rate, $debug|


outgoing :: SetTXRate($rate)
 -> [0] output


es :: LinkStat($debug);


gw :: WINGGatewaySelector(IP $ip, 
                         PERIOD 15000,
                         EXPIRE 30000,
                         LT lt, 
                         ARP arp,
                         DEBUG $debug);


set_gateway :: WINGSetGateway(SEL gw);


forwarder :: WINGForwarder(IP $ip, ARP arp);


querier :: WINGQuerier(IP $ip,
                       QUERY_WAIT 5,
                       TIME_BEFORE_SWITCH 5,
                       LT lt, 
                       ARP arp,
                       DEBUG $debug);


lb :: WINGLocalBroadcast(IP $ip,
                         LT lt, 
                         ARP arp);


query_forwarder :: WINGMetricFlood(IP $ip, 
                                  LT lt, 
                                  ARP arp,
                                  DEBUG $debug);


query_responder :: WINGQueryResponder(IP $ip,
                                     LT lt, 
                                     ARP arp,
                                     DEBUG $debug);


gw_responder ::  WINGGatewayResponder(PERIOD 15000,
                                 SEL gw, 
                                 LT lt, 
                                 MF query_forwarder);


gw -> outgoing;
query_responder -> outgoing;
query_forwarder -> outgoing;


query_forwarder [1] -> [0] query_responder;


input [1] 
-> host_cl :: IPClassifier(dst host $bcast or dst host 255.255.255.255, dst net $ip mask $nm, -)
-> lb 
-> [1] output;


host_cl [1] 
-> querier
-> [1] output;


host_cl [2] -> [0] set_gateway [0] -> querier;


forwarder[0] 
  -> dt ::DecIPTTL
  -> [1] output;


dt[1] 
-> ICMPError($ip, timeexceeded, 0) 
-> querier;


querier [1] -> [0] query_forwarder;
es -> SetTimestamp() -> [0] output;


forwarder[1] // IP packets to me
  -> WINGStripHeader()
  -> check_ip :: CheckIPHeader(CHECKSUM false)
  -> from_gw_cl :: IPClassifier(src net $ip mask $nm, -)
  -> [2] output;


from_gw_cl [1] -> [1] set_gateway [1] -> [2] output;


input [0]
  -> ncl :: Classifier(15/01 , // forwarder
                       15/02 , // queries
                       15/03 , // replies
                       15/04 , // es
                       15/05 , // gw
                       15/06 , // bcast
                      );
 
 
 ncl[0] -> forwarder;
 ncl[1] -> [1] query_forwarder;
 ncl[2] -> [1] query_responder;
 ncl[3] -> es;
 ncl[4] -> gw;
 ncl[5] -> Strip(22) -> check_ip;

}

