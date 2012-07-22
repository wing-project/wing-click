rates_1 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 00:15:6D:84:13:5D 12 18 24 36 48 72 96 108);
rates_2 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 00:15:6D:84:13:5E 12 18 24 36 48 72 96 108);
lt :: LinkTableMulti(IP 6.132.19.93, IFACES " 1 2", BETA 80, DEBUG false);
metric :: WINGETTMetric(LT lt, DEBUG false);
arp :: ARPTableMulti();

elementclass EtherSplit {
input -> cl :: Classifier(6/00156D84135D, 6/00156D84135E);
cl[0] -> [0] output;
cl[1] -> [1] output;
}

elementclass LinkStat {
    $debug|
input -> ps :: PaintSwitch();
es_1 :: WINGLinkStat(IFNAME wlan0-1,
                                  ETH 00:15:6D:84:13:5D, 
                                  IFID 1, 
                                  CHANNEL 2462, 
                                  RATES rates_1,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  METRIC metric,
                                  LT lt,
                                  ARP arp,
                                  DEBUG $debug);
ps[0] -> es_1 -> output;
es_2 :: WINGLinkStat(IFNAME wlan1-1,
                                  ETH 00:15:6D:84:13:5E, 
                                  IFID 2, 
                                  CHANNEL 5240, 
                                  RATES rates_2,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  METRIC metric,
                                  LT lt,
                                  ARP arp,
                                  DEBUG $debug);
ps[1] -> es_2 -> output;
}

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

elementclass RateControl {
  $rate, $rates|

  rate_control :: Minstrel(OFFSET 4, RT $rates);

  input -> rate_control -> output;
  input [1] -> [1] rate_control [1] -> [1] output;

};

elementclass LinkScheduler {
  $lt, $arp|

  input -> FullNoteQueue() -> output;
  
};

control :: ControlSocket("TCP", 7777);

// has one input and one output
// takes and spits out ip packets
elementclass LinuxIPHost {
    $dev, $ip, $nm|
  input -> KernelTun($ip/$nm, MTU 1500, DEVNAME $dev, BURST 20) 
  -> CheckIPHeader(CHECKSUM false)
  -> output;
}

elementclass SniffDevice {
    $device|
  from_dev :: FromDevice($device, PROMISC false, OUTBOUND true, SNIFFER false) 
  -> output;
  input -> to_dev :: ToDevice($device);
}

wr :: WingRouter (6.132.19.93, 255.0.0.0, 6.255.255.255, 12, false);

dyn :: DynGW(DEVNAME wing-mesh, SEL wr/gw);

linux_ip_host :: LinuxIPHost(wing-mesh, 6.132.19.93, 255.0.0.0) -> [1] wr;

rc_split :: EtherSplit(); 
sl_split :: EtherSplit(); 

wr [0] -> WINGSetHeader() -> sl_split; // queries, replies, bcast_stats
wr [1] -> WINGSetHeader() -> rc_split; // data 
wr [2] -> linux_ip_host;

sniff_dev_1 :: SniffDevice(wlan0-1);
rc_1 :: RateControl(12, rates_1);
ls_1 :: LinkScheduler(lt, arp);
outgoing_1 :: PrioSched()
-> SetTXPower(POWER 60) 
-> RadiotapEncap() 
-> sniff_dev_1;

sl_split[0] -> FullNoteQueue() -> WifiEncap(0x0, 00:00:00:00:00:00) -> [0] outgoing_1;
rc_split[0] -> ls_1 -> WifiEncap(0x0, 00:00:00:00:00:00) -> rc_1 -> [1] outgoing_1;

sniff_dev_2 :: SniffDevice(wlan1-1);
rc_2 :: RateControl(12, rates_2);
ls_2 :: LinkScheduler(lt, arp);
outgoing_2 :: PrioSched()
-> SetTXPower(POWER 60) 
-> RadiotapEncap() 
-> sniff_dev_2;

sl_split[1] -> FullNoteQueue() -> WifiEncap(0x0, 00:00:00:00:00:00) -> [0] outgoing_2;
rc_split[1] -> ls_2 -> WifiEncap(0x0, 00:00:00:00:00:00) -> rc_2 -> [1] outgoing_2;

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
-> HostEtherFilter(00:15:6D:84:13:5D, DROP_OTHER true, DROP_OWN true)
-> Paint(0)
-> cl;

tx_filter_1[1] -> [1] rc_1 [1] -> Discard();

sniff_dev_2
-> RadiotapDecap
-> FilterPhyErr()
-> Classifier(0/08%0c) //data
-> tx_filter_2 :: FilterTX()
-> WifiDupeFilter() 
-> WifiDecap()
-> HostEtherFilter(00:15:6D:84:13:5E, DROP_OTHER true, DROP_OWN true)
-> Paint(1)
-> cl;

tx_filter_2[1] -> [1] rc_2 [1] -> Discard();

