rates_1 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 06:0C:42:23:AA:99 12 18 24 36 48 72 96 108);
channels_1 :: AvailableChannels(DEFAULT 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462, 06:0C:42:23:AA:99 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462);
dev_1 :: DevInfo(ETH 06:0C:42:23:AA:99, CHANNEL 2412, CHANNELS channels_1, RATES rates_1);
rates_2 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 00:0B:6B:56:7E:5F 12 18 24 36 48 72 96 108);
channels_2 :: AvailableChannels(DEFAULT 5180 5200 5220 5240 5260 5280 5300 5320, 00:0B:6B:56:7E:5F 5180 5200 5220 5240 5260 5280 5300 5320);
dev_2 :: DevInfo(ETH 00:0B:6B:56:7E:5F, CHANNEL 5180, CHANNELS channels_2, RATES rates_2);
lt :: LinkTableMulti(IP 6.35.170.153, DEV " dev_1 dev_2", WCETT true, BETA 80);
metric :: WINGETTMetric(LT lt);


arp :: ARPTableMulti();

elementclass EtherSplit {
input -> cl :: Classifier(6/060C4223AA99, 6/000B6B567E5F);
cl[0] -> [0] output;
cl[1] -> [1] output;
}

elementclass LinkStat {
    $debug|
input -> ps :: PaintSwitch();
es_1 :: WINGLinkStat(IP 6.35.170.153, 
                                  DEV dev_1,
                                  PERIOD 36000,
                                  TAU 360000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  ARP arp,
                                  LT lt,
                                  METRIC metric,
                                  DEBUG $debug);
ps[0] -> es_1 -> output;
es_2 :: WINGLinkStat(IP 6.35.170.153, 
                                  DEV dev_2,
                                  PERIOD 36000,
                                  TAU 360000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  ARP arp,
                                  LT lt,
                                  METRIC metric,
                                  DEBUG $debug);
ps[1] -> es_2 -> output;
}

elementclass WingRouter {
  $ip, $nm, $rate, $debug|


outgoing :: SetTXRate($rate)
 -> [0] output

es :: LinkStat($debug);


gw :: WINGGatewaySelector(IP $ip, 
                         PERIOD 15000,
                         JITTER 1000,
                         EXPIRE 30000,
                         LT lt, 
                         ARP arp,
                         DEBUG $debug);

track_flows :: WINGTrackFlows();


set_gateway :: WINGSetGateway(SEL gw);


forwarder :: WINGForwarder(IP $ip, 
                          LT lt, 
                          ARP arp,
                          DEBUG $debug);


querier :: WINGQuerier(QUERY_WAIT 5,
                         TIME_BEFORE_SWITCH 5,
                         FWD forwarder,
                         LT lt, 
                         DEBUG $debug);


query_forwarder :: WINGMetricFlood(IP $ip, 
                                  LT lt, 
                                  ARP arp,
                                  DEBUG $debug);


query_responder :: WINGQueryResponder(IP $ip,
                                     LT lt, 
                                     ARP arp,
                                     DEBUG $debug);


reply_forwarder :: WINGReplyForwarder(IP $ip, 
                                     LT lt, 
                                     ARP arp,
                                     DEBUG $debug);


gw_responder ::  WINGGatewayResponder(IP $ip,
                                 PERIOD 15000,
                                 SEL gw, 
                                 LT lt, 
                                 RESPONDER query_responder,
                                 DEBUG $debug);


gw -> outgoing;
gw_responder -> outgoing;
query_responder -> outgoing;
reply_forwarder -> outgoing;
query_forwarder -> outgoing;

query_forwarder [1] -> query_responder;


input [1] 
-> host_cl :: IPClassifier(dst net $ip mask $nm, -)
-> querier
-> [1] output;


querier -> [1] output;


host_cl [1] -> set_gateway -> [0] track_flows [0] -> querier;


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
  -> CheckIPHeader(CHECKSUM false)
  -> from_gw_cl :: IPClassifier(src net $ip mask $nm, -)
  -> [2] output;


from_gw_cl [1] -> [1] track_flows [1] -> [2] output;


input [0]
  -> ncl :: Classifier(15/01 , // forwarder
                       15/02 , // queries
                       15/03 , // replies
                       15/04 , // es
                       15/05 , // gw
                      );
 
 
 ncl[0] -> forwarder;
 ncl[1] -> [1] query_forwarder
 ncl[2] -> reply_forwarder;
 ncl[3] -> es;
 ncl[4] -> gw;

}
elementclass RateControl {
  $rate, $rates|

  rate_control :: SetTXRate(RATE $rate);

  input -> rate_control -> output;
  input [1] -> Discard();

};
elementclass LinkScheduler {
  $lt, $arp|

  input -> FullNoteQueue(10) -> output;
  
};


control :: ControlSocket("TCP", 7777);

// has one input and one output
// takes and spits out ip packets
elementclass LinuxIPHost {
    $dev, $ip, $nm|
  input -> KernelTun($ip/$nm, MTU 1500, DEVNAME $dev) 
  -> CheckIPHeader(CHECKSUM false)
  -> output;
}

elementclass SniffDevice {
    $device, $promisc, $outbound|
  from_dev :: FromDevice($device, PROMISC $promisc, OUTBOUND $outbound) 
  -> output;
  input -> to_dev :: ToDevice($device);
}

wr :: WingRouter (6.35.170.153, 255.0.0.0, 12, true);

linux_ip_host :: LinuxIPHost(wing-mesh, 6.35.170.153, 255.0.0.0) -> [1] wr;

rc_split :: EtherSplit(); 
sl_split :: EtherSplit(); 

wr [0] -> sl_split; // queries, replies, bcast_stats
wr [1] -> rc_split; 
wr [2] -> linux_ip_host;
sniff_dev_1 :: SniffDevice(ath1, false, true);
rc_1 :: RateControl(12, rates_1);
ls_1 :: LinkScheduler(lt, arp);
outgoing_1 :: PrioSched()
-> WINGSetHeader() 
-> WifiEncap(0x0, 00:00:00:00:00:00) 
-> SetTXPower(POWER 60) 
-> RadiotapEncap() 
-> sniff_dev_1;

sl_split[0] -> FullNoteQueue() -> [0] outgoing_1;
rc_split[0] -> ls_1 -> rc_1 -> [1] outgoing_1;
sniff_dev_2 :: SniffDevice(ath2, false, true);
rc_2 :: RateControl(12, rates_2);
ls_2 :: LinkScheduler(lt, arp);
outgoing_2 :: PrioSched()
-> WINGSetHeader() 
-> WifiEncap(0x0, 00:00:00:00:00:00) 
-> SetTXPower(POWER 60) 
-> RadiotapEncap() 
-> sniff_dev_2;

sl_split[1] -> FullNoteQueue() -> [0] outgoing_2;
rc_split[1] -> ls_2 -> rc_2 -> [1] outgoing_2;
cl :: Classifier(12/06AA) // this protocol's ethertype
-> WINGCheckHeader()
-> wr;


sniff_dev_1
-> RadiotapDecap
-> FilterPhyErr()
-> Classifier(0/08%0c) //data
-> tx_filter_1 :: FilterTX()
-> WifiDupeFilter() 
-> WifiDecap()
-> HostEtherFilter(06:0C:42:23:AA:99, DROP_OTHER true, DROP_OWN true)
-> Paint(0)
-> cl;

tx_filter_1[1] -> [1] rc_1;
sniff_dev_2
-> RadiotapDecap
-> FilterPhyErr()
-> Classifier(0/08%0c) //data
-> tx_filter_2 :: FilterTX()
-> WifiDupeFilter() 
-> WifiDecap()
-> HostEtherFilter(00:0B:6B:56:7E:5F, DROP_OTHER true, DROP_OWN true)
-> Paint(1)
-> cl;

tx_filter_2[1] -> [1] rc_2;
