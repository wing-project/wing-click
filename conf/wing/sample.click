# 1 "/tmp/wing-mesh.click"
rates_1 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 06:0C:42:23:AA:6F 12 18 24 36 48 72 96 108);
# 2 "/tmp/wing-mesh.click"
channels_1 :: AvailableChannels(DEFAULT 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462, 06:0C:42:23:AA:6F 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462);
# 3 "/tmp/wing-mesh.click"
dev_1 :: DevInfo(ETH 06:0C:42:23:AA:6F, IFACE 1, CHANNEL 2412, CHANNELS channels_1, RATES rates_1);
# 4 "/tmp/wing-mesh.click"
rates_2 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 00:0C:42:23:AA:81 12 18 24 36 48 72 96 108);
# 5 "/tmp/wing-mesh.click"
channels_2 :: AvailableChannels(DEFAULT 5180 5200 5220 5240 5260 5280 5300 5320, 00:0C:42:23:AA:81 5180 5200 5220 5240 5260 5280 5300 5320);
# 6 "/tmp/wing-mesh.click"
dev_2 :: DevInfo(ETH 00:0C:42:23:AA:81, IFACE 2, CHANNEL 5180, CHANNELS channels_2, RATES rates_2);
# 7 "/tmp/wing-mesh.click"
lt :: LinkTableMulti(IP 6.35.170.111, IFACES " 1 2", BETA 80);
# 8 "/tmp/wing-mesh.click"
metric :: WINGETTMetric(LT lt);
# 9 "/tmp/wing-mesh.click"
arp :: ARPTableMulti;
# 179 "/tmp/wing-mesh.click"
control :: ControlSocket("TCP", 7777);
# 181 "/tmp/wing-mesh.click"
err :: WifiDupeFilter;
# 182 "/tmp/wing-mesh.click"
WifiDecap@12 :: WifiDecap;
# 183 "/tmp/wing-mesh.click"
HostEtherFilter@13 :: HostEtherFilter(00:0C:42:23:AA:81, DROP_OTHER true, DROP_OWN true);
# 184 "/tmp/wing-mesh.click"
WINGRouteResponder@14 :: WINGRouteResponder(IP 6.35.170.111, LT lt, ARP arp);
# 185 "/tmp/wing-mesh.click"
Discard@15 :: Discard;
# 210 "/tmp/wing-mesh.click"
WINGSetHeader@20 :: WINGSetHeader;
# 210 "/tmp/wing-mesh.click"
WINGSetHeader@21 :: WINGSetHeader;
# 217 "/tmp/wing-mesh.click"
outgoing_1 :: PrioSched;
# 218 "/tmp/wing-mesh.click"
SetTXPower@26 :: SetTXPower(POWER 60);
# 219 "/tmp/wing-mesh.click"
RadiotapEncap@27 :: RadiotapEncap;
# 222 "/tmp/wing-mesh.click"
FullNoteQueue@28 :: FullNoteQueue;
# 222 "/tmp/wing-mesh.click"
WifiEncap@29 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 223 "/tmp/wing-mesh.click"
WifiEncap@30 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 228 "/tmp/wing-mesh.click"
outgoing_2 :: PrioSched;
# 229 "/tmp/wing-mesh.click"
SetTXPower@35 :: SetTXPower(POWER 60);
# 230 "/tmp/wing-mesh.click"
RadiotapEncap@36 :: RadiotapEncap;
# 233 "/tmp/wing-mesh.click"
FullNoteQueue@37 :: FullNoteQueue;
# 233 "/tmp/wing-mesh.click"
WifiEncap@38 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 234 "/tmp/wing-mesh.click"
WifiEncap@39 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 236 "/tmp/wing-mesh.click"
cl :: Classifier(12/06AA);
# 236 "/tmp/wing-mesh.click"
WINGCheckHeader@41 :: WINGCheckHeader;
# 241 "/tmp/wing-mesh.click"
RadiotapDecap@42 :: RadiotapDecap;
# 242 "/tmp/wing-mesh.click"
FilterPhyErr@43 :: FilterPhyErr;
# 243 "/tmp/wing-mesh.click"
Classifier@44 :: Classifier(0/08%0c);
# 243 "/tmp/wing-mesh.click"
tx_filter_1 :: FilterTX;
# 245 "/tmp/wing-mesh.click"
WifiDupeFilter@46 :: WifiDupeFilter;
# 246 "/tmp/wing-mesh.click"
WifiDecap@47 :: WifiDecap;
# 247 "/tmp/wing-mesh.click"
HostEtherFilter@48 :: HostEtherFilter(06:0C:42:23:AA:6F, DROP_OTHER true, DROP_OWN true);
# 248 "/tmp/wing-mesh.click"
Paint@49 :: Paint(0);
# 254 "/tmp/wing-mesh.click"
RadiotapDecap@50 :: RadiotapDecap;
# 255 "/tmp/wing-mesh.click"
FilterPhyErr@51 :: FilterPhyErr;
# 256 "/tmp/wing-mesh.click"
Classifier@52 :: Classifier(0/08%0c);
# 256 "/tmp/wing-mesh.click"
tx_filter_2 :: FilterTX;
# 258 "/tmp/wing-mesh.click"
WifiDupeFilter@54 :: WifiDupeFilter;
# 259 "/tmp/wing-mesh.click"
WifiDecap@55 :: WifiDecap;
# 260 "/tmp/wing-mesh.click"
HostEtherFilter@56 :: HostEtherFilter(00:0C:42:23:AA:81, DROP_OTHER true, DROP_OWN true);
# 261 "/tmp/wing-mesh.click"
Paint@57 :: Paint(1);
# 46 "/tmp/wing-mesh.click"
wr/outgoing :: SetTXRate(12);
# 19 "/tmp/wing-mesh.click"
wr/es/ps :: PaintSwitch;
# 20 "/tmp/wing-mesh.click"
wr/es/es_1 :: WINGLinkStat(IP 6.35.170.111, 
                                  DEV dev_1,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  ARP arp,
                                  LT lt,
                                  METRIC metric,
                                  DEBUG false);
# 30 "/tmp/wing-mesh.click"
wr/es/es_2 :: WINGLinkStat(IP 6.35.170.111, 
                                  DEV dev_2,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  ARP arp,
                                  LT lt,
                                  METRIC metric,
                                  DEBUG false);
# 53 "/tmp/wing-mesh.click"
wr/gw :: WINGGatewaySelector(IP 6.35.170.111, 
                         PERIOD 15000,
                         EXPIRE 30000,
                         LT lt, 
                         ARP arp,
                         DEBUG false);
# 61 "/tmp/wing-mesh.click"
wr/track_flows :: WINGTrackFlows;
# 64 "/tmp/wing-mesh.click"
wr/set_gateway :: WINGSetGateway(SEL gw);
# 67 "/tmp/wing-mesh.click"
wr/forwarder :: WINGForwarder(IP 6.35.170.111, 
                          ARP arp,
                          DEBUG false);
# 72 "/tmp/wing-mesh.click"
wr/querier :: WINGQuerier(QUERY_WAIT 5,
                         TIME_BEFORE_SWITCH 5,
			 IP 6.35.170.111,
			 ARP arp,
                         LT lt, 
                         DEBUG false);
# 80 "/tmp/wing-mesh.click"
wr/query_forwarder :: WINGMetricFlood(IP 6.35.170.111, 
                                  LT lt, 
                                  ARP arp,
                                  DEBUG false);
# 86 "/tmp/wing-mesh.click"
wr/query_responder :: WINGQueryResponder(IP 6.35.170.111,
                                     LT lt, 
                                     ARP arp,
                                     DEBUG false);
# 92 "/tmp/wing-mesh.click"
wr/gw_responder :: WINGGatewayResponder(IP 6.35.170.111,
                                 PERIOD 15000,
                                 SEL gw, 
                                 LT lt, 
                                 RESPONDER query_responder,
                                 DEBUG false);
# 110 "/tmp/wing-mesh.click"
wr/host_cl :: IPClassifier(dst net 6.35.170.111 mask 255.0.0.0, -);
# 122 "/tmp/wing-mesh.click"
wr/dt :: DecIPTTL;
# 127 "/tmp/wing-mesh.click"
wr/ICMPError@13 :: ICMPError(6.35.170.111, timeexceeded, 0);
# 132 "/tmp/wing-mesh.click"
wr/SetTimestamp@14 :: SetTimestamp;
# 135 "/tmp/wing-mesh.click"
wr/WINGStripHeader@15 :: WINGStripHeader;
# 137 "/tmp/wing-mesh.click"
wr/CheckIPHeader@16 :: CheckIPHeader(CHECKSUM false);
# 138 "/tmp/wing-mesh.click"
wr/from_gw_cl :: IPClassifier(src net 6.35.170.111 mask 255.0.0.0, -);
# 146 "/tmp/wing-mesh.click"
wr/ncl :: Classifier(15/01 , // forwarder
                       15/02 , // queries
                       15/03 , // replies
                       15/04 , // es
                       15/05 , // gw
                      );
# 191 "/tmp/wing-mesh.click"
linux_ip_host/KernelTun@1 :: KernelTun(6.35.170.111/255.0.0.0, MTU 1500, DEVNAME wing-mesh);
# 192 "/tmp/wing-mesh.click"
linux_ip_host/CheckIPHeader@2 :: CheckIPHeader(CHECKSUM false);
# 12 "/tmp/wing-mesh.click"
rc_split/cl :: Classifier(6/060C4223AA6F, 6/000C4223AA81);
# 12 "/tmp/wing-mesh.click"
sl_split/cl :: Classifier(6/060C4223AA6F, 6/000C4223AA81);
# 198 "/tmp/wing-mesh.click"
sniff_dev_1/from_dev :: FromDevice(ath1, PROMISC false, OUTBOUND true, SNIFFER false);
# 200 "/tmp/wing-mesh.click"
sniff_dev_1/to_dev :: ToDevice(ath1);
# 165 "/tmp/wing-mesh.click"
rc_1/rate_control :: Minstrel(OFFSET 4, RT rates_1);
# 175 "/tmp/wing-mesh.click"
ls_1/FullNoteQueue@1 :: FullNoteQueue(10);
# 198 "/tmp/wing-mesh.click"
sniff_dev_2/from_dev :: FromDevice(ath2, PROMISC false, OUTBOUND true, SNIFFER false);
# 200 "/tmp/wing-mesh.click"
sniff_dev_2/to_dev :: ToDevice(ath2);
# 165 "/tmp/wing-mesh.click"
rc_2/rate_control :: Minstrel(OFFSET 4, RT rates_2);
# 175 "/tmp/wing-mesh.click"
ls_2/FullNoteQueue@1 :: FullNoteQueue(10);
# 0 "<click-align>"
Align@click_align@80 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@81 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@82 :: Align(4, 0);
# 0 "<click-align>"
AlignmentInfo@click_align@83 :: AlignmentInfo(cl  4 2,
  Classifier@44  4 0,
  Classifier@52  4 0,
  wr/CheckIPHeader@16  4 0,
  wr/ncl  4 2,
  linux_ip_host/CheckIPHeader@2  4 0,
  rc_split/cl  4 2,
  sl_split/cl  4 0);
# 219 ""
rc_split/cl [1] -> ls_2/FullNoteQueue@1
    -> WifiEncap@39
    -> rc_2/rate_control
    -> [1] outgoing_2;
tx_filter_2 [1] -> [1] rc_2/rate_control;
tx_filter_1 [1] -> [1] rc_1/rate_control;
wr/track_flows [1] -> linux_ip_host/KernelTun@1
    -> linux_ip_host/CheckIPHeader@2
    -> wr/host_cl
    -> wr/querier
    -> WINGSetHeader@21
    -> rc_split/cl
    -> ls_1/FullNoteQueue@1
    -> WifiEncap@30
    -> rc_1/rate_control
    -> [1] outgoing_1;
wr/ncl [3] -> wr/es/ps
    -> wr/es/es_1
    -> wr/SetTimestamp@14
    -> WINGSetHeader@20
    -> Align@click_align@82
    -> sl_split/cl
    -> FullNoteQueue@28
    -> WifiEncap@29
    -> outgoing_1
    -> SetTXPower@26
    -> RadiotapEncap@27
    -> sniff_dev_1/to_dev;
rc_2/rate_control [1] -> err
    -> WifiDecap@12
    -> HostEtherFilter@13
    -> WINGRouteResponder@14
    -> Discard@15;
sniff_dev_2/from_dev -> RadiotapDecap@50
    -> FilterPhyErr@51
    -> Align@click_align@81
    -> Classifier@52
    -> tx_filter_2
    -> WifiDupeFilter@54
    -> WifiDecap@55
    -> HostEtherFilter@56
    -> Paint@57
    -> cl
    -> WINGCheckHeader@41
    -> wr/ncl
    -> wr/forwarder
    -> wr/dt
    -> WINGSetHeader@21;
rc_1/rate_control [1] -> err;
sniff_dev_1/from_dev -> RadiotapDecap@42
    -> FilterPhyErr@43
    -> Align@click_align@80
    -> Classifier@44
    -> tx_filter_1
    -> WifiDupeFilter@46
    -> WifiDecap@47
    -> HostEtherFilter@48
    -> Paint@49
    -> cl;
sl_split/cl [1] -> FullNoteQueue@37
    -> WifiEncap@38
    -> outgoing_2
    -> SetTXPower@35
    -> RadiotapEncap@36
    -> sniff_dev_2/to_dev;
wr/es/ps [1] -> wr/es/es_2
    -> wr/SetTimestamp@14;
wr/gw_responder -> wr/outgoing
    -> WINGSetHeader@20;
wr/query_forwarder [1] -> wr/query_responder
    -> wr/outgoing;
wr/host_cl [1] -> wr/set_gateway
    -> wr/track_flows
    -> wr/querier;
wr/ncl [4] -> wr/gw
    -> wr/outgoing;
wr/dt [1] -> wr/ICMPError@13
    -> wr/querier;
wr/querier [1] -> wr/query_forwarder
    -> wr/outgoing;
wr/ncl [2] -> [1] wr/query_responder;
wr/ncl [1] -> [1] wr/query_forwarder;
wr/forwarder [1] -> wr/WINGStripHeader@15
    -> wr/CheckIPHeader@16
    -> wr/from_gw_cl
    -> linux_ip_host/KernelTun@1;
wr/from_gw_cl [1] -> [1] wr/track_flows;
