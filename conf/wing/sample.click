# 1 "/tmp/wing-mesh.click"
rates_1 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 06:0C:42:23:AA:6F 12 18 24 36 48 72 96 108);
# 2 "/tmp/wing-mesh.click"
channels_1 :: AvailableChannels(DEFAULT 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462, 06:0C:42:23:AA:6F 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462);
# 3 "/tmp/wing-mesh.click"
dev_1 :: DevInfo(ETH 06:0C:42:23:AA:6F, CHANNEL 2412, CHANNELS channels_1, RATES rates_1);
# 4 "/tmp/wing-mesh.click"
rates_2 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 00:0C:42:23:AA:81 12 18 24 36 48 72 96 108);
# 5 "/tmp/wing-mesh.click"
channels_2 :: AvailableChannels(DEFAULT 5180 5200 5220 5240 5260 5280 5300 5320, 00:0C:42:23:AA:81 5180 5200 5220 5240 5260 5280 5300 5320);
# 6 "/tmp/wing-mesh.click"
dev_2 :: DevInfo(ETH 00:0C:42:23:AA:81, CHANNEL 5180, CHANNELS channels_2, RATES rates_2);
# 7 "/tmp/wing-mesh.click"
lt :: LinkTableMulti(IP 6.35.170.111, DEV " dev_1 dev_2", WCETT true, BETA 80);
# 8 "/tmp/wing-mesh.click"
metric :: WINGETTMetric(LT lt);
# 9 "/tmp/wing-mesh.click"
arp :: ARPTableMulti;
# 180 "/tmp/wing-mesh.click"
control :: ControlSocket("TCP", 7777);
# 205 "/tmp/wing-mesh.click"
WINGSetHeader@15 :: WINGSetHeader;
# 205 "/tmp/wing-mesh.click"
WINGSetHeader@16 :: WINGSetHeader;
# 212 "/tmp/wing-mesh.click"
outgoing_1 :: PrioSched;
# 213 "/tmp/wing-mesh.click"
SetTXPower@21 :: SetTXPower(POWER 60);
# 214 "/tmp/wing-mesh.click"
RadiotapEncap@22 :: RadiotapEncap;
# 217 "/tmp/wing-mesh.click"
FullNoteQueue@23 :: FullNoteQueue;
# 217 "/tmp/wing-mesh.click"
WifiEncap@24 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 218 "/tmp/wing-mesh.click"
WifiEncap@25 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 223 "/tmp/wing-mesh.click"
outgoing_2 :: PrioSched;
# 224 "/tmp/wing-mesh.click"
SetTXPower@30 :: SetTXPower(POWER 60);
# 225 "/tmp/wing-mesh.click"
RadiotapEncap@31 :: RadiotapEncap;
# 228 "/tmp/wing-mesh.click"
FullNoteQueue@32 :: FullNoteQueue;
# 228 "/tmp/wing-mesh.click"
WifiEncap@33 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 229 "/tmp/wing-mesh.click"
WifiEncap@34 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 231 "/tmp/wing-mesh.click"
cl :: Classifier(12/06AA);
# 231 "/tmp/wing-mesh.click"
WINGCheckHeader@36 :: WINGCheckHeader;
# 236 "/tmp/wing-mesh.click"
RadiotapDecap@37 :: RadiotapDecap;
# 237 "/tmp/wing-mesh.click"
FilterPhyErr@38 :: FilterPhyErr;
# 238 "/tmp/wing-mesh.click"
Classifier@39 :: Classifier(0/08%0c);
# 238 "/tmp/wing-mesh.click"
tx_filter_1 :: FilterTX;
# 240 "/tmp/wing-mesh.click"
WifiDupeFilter@41 :: WifiDupeFilter;
# 241 "/tmp/wing-mesh.click"
WifiDecap@42 :: WifiDecap;
# 242 "/tmp/wing-mesh.click"
HostEtherFilter@43 :: HostEtherFilter(06:0C:42:23:AA:6F, DROP_OTHER true, DROP_OWN true);
# 243 "/tmp/wing-mesh.click"
Paint@44 :: Paint(0);
# 249 "/tmp/wing-mesh.click"
RadiotapDecap@45 :: RadiotapDecap;
# 250 "/tmp/wing-mesh.click"
FilterPhyErr@46 :: FilterPhyErr;
# 251 "/tmp/wing-mesh.click"
Classifier@47 :: Classifier(0/08%0c);
# 251 "/tmp/wing-mesh.click"
tx_filter_2 :: FilterTX;
# 253 "/tmp/wing-mesh.click"
WifiDupeFilter@49 :: WifiDupeFilter;
# 254 "/tmp/wing-mesh.click"
WifiDecap@50 :: WifiDecap;
# 255 "/tmp/wing-mesh.click"
HostEtherFilter@51 :: HostEtherFilter(00:0C:42:23:AA:81, DROP_OTHER true, DROP_OWN true);
# 256 "/tmp/wing-mesh.click"
Paint@52 :: Paint(1);
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
                         JITTER 1000,
                         EXPIRE 30000,
                         LT lt, 
                         ARP arp,
                         DEBUG false);
# 62 "/tmp/wing-mesh.click"
wr/track_flows :: WINGTrackFlows;
# 65 "/tmp/wing-mesh.click"
wr/set_gateway :: WINGSetGateway(SEL gw);
# 68 "/tmp/wing-mesh.click"
wr/forwarder :: WINGForwarder(IP 6.35.170.111, 
                          LT lt, 
                          ARP arp,
                          DEBUG false);
# 74 "/tmp/wing-mesh.click"
wr/querier :: WINGQuerier(QUERY_WAIT 5,
                         TIME_BEFORE_SWITCH 5,
                         FWD forwarder,
                         LT lt, 
                         DEBUG false);
# 81 "/tmp/wing-mesh.click"
wr/query_forwarder :: WINGMetricFlood(IP 6.35.170.111, 
                                  LT lt, 
                                  ARP arp,
                                  DEBUG false);
# 87 "/tmp/wing-mesh.click"
wr/query_responder :: WINGQueryResponder(IP 6.35.170.111,
                                     LT lt, 
                                     ARP arp,
                                     DEBUG false);
# 93 "/tmp/wing-mesh.click"
wr/gw_responder :: WINGGatewayResponder(IP 6.35.170.111,
                                 PERIOD 15000,
                                 SEL gw, 
                                 LT lt, 
                                 RESPONDER query_responder,
                                 DEBUG false);
# 111 "/tmp/wing-mesh.click"
wr/host_cl :: IPClassifier(dst net 6.35.170.111 mask 255.0.0.0, -);
# 123 "/tmp/wing-mesh.click"
wr/dt :: DecIPTTL;
# 128 "/tmp/wing-mesh.click"
wr/ICMPError@13 :: ICMPError(6.35.170.111, timeexceeded, 0);
# 133 "/tmp/wing-mesh.click"
wr/SetTimestamp@14 :: SetTimestamp;
# 136 "/tmp/wing-mesh.click"
wr/WINGStripHeader@15 :: WINGStripHeader;
# 138 "/tmp/wing-mesh.click"
wr/CheckIPHeader@16 :: CheckIPHeader(CHECKSUM false);
# 139 "/tmp/wing-mesh.click"
wr/from_gw_cl :: IPClassifier(src net 6.35.170.111 mask 255.0.0.0, -);
# 147 "/tmp/wing-mesh.click"
wr/ncl :: Classifier(15/01 , // forwarder
                       15/02 , // queries
                       15/03 , // replies
                       15/04 , // es
                       15/05 , // gw
                      );
# 186 "/tmp/wing-mesh.click"
linux_ip_host/KernelTun@1 :: KernelTun(6.35.170.111/255.0.0.0, MTU 1500, DEVNAME wing-mesh);
# 187 "/tmp/wing-mesh.click"
linux_ip_host/CheckIPHeader@2 :: CheckIPHeader(CHECKSUM false);
# 12 "/tmp/wing-mesh.click"
rc_split/cl :: Classifier(6/060C4223AA6F, 6/000C4223AA81);
# 12 "/tmp/wing-mesh.click"
sl_split/cl :: Classifier(6/060C4223AA6F, 6/000C4223AA81);
# 193 "/tmp/wing-mesh.click"
sniff_dev_1/from_dev :: FromDevice(ath1, PROMISC false, OUTBOUND true, SNIFFER false);
# 195 "/tmp/wing-mesh.click"
sniff_dev_1/to_dev :: ToDevice(ath1);
# 166 "/tmp/wing-mesh.click"
rc_1/rate_control :: Minstrel(OFFSET 4, RT rates_1);
# 176 "/tmp/wing-mesh.click"
ls_1/FullNoteQueue@1 :: FullNoteQueue(10);
# 193 "/tmp/wing-mesh.click"
sniff_dev_2/from_dev :: FromDevice(ath2, PROMISC false, OUTBOUND true, SNIFFER false);
# 195 "/tmp/wing-mesh.click"
sniff_dev_2/to_dev :: ToDevice(ath2);
# 166 "/tmp/wing-mesh.click"
rc_2/rate_control :: Minstrel(OFFSET 4, RT rates_2);
# 176 "/tmp/wing-mesh.click"
ls_2/FullNoteQueue@1 :: FullNoteQueue(10);
# 0 "<click-align>"
Align@click_align@75 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@76 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@77 :: Align(4, 0);
# 0 "<click-align>"
AlignmentInfo@click_align@78 :: AlignmentInfo(cl  4 2,
  Classifier@39  4 0,
  Classifier@47  4 0,
  wr/CheckIPHeader@16  4 0,
  wr/ncl  4 2,
  linux_ip_host/CheckIPHeader@2  4 0,
  rc_split/cl  4 2,
  sl_split/cl  4 0);
# 210 ""
rc_split/cl [1] -> ls_2/FullNoteQueue@1
    -> WifiEncap@34
    -> rc_2/rate_control
    -> [1] outgoing_2;
tx_filter_2 [1] -> [1] rc_2/rate_control;
tx_filter_1 [1] -> [1] rc_1/rate_control;
wr/track_flows [1] -> linux_ip_host/KernelTun@1
    -> linux_ip_host/CheckIPHeader@2
    -> wr/host_cl
    -> wr/querier
    -> WINGSetHeader@16
    -> rc_split/cl
    -> ls_1/FullNoteQueue@1
    -> WifiEncap@25
    -> rc_1/rate_control
    -> [1] outgoing_1;
wr/ncl [3] -> wr/es/ps
    -> wr/es/es_1
    -> wr/SetTimestamp@14
    -> WINGSetHeader@15
    -> Align@click_align@77
    -> sl_split/cl
    -> FullNoteQueue@23
    -> WifiEncap@24
    -> outgoing_1
    -> SetTXPower@21
    -> RadiotapEncap@22
    -> sniff_dev_1/to_dev;
sniff_dev_2/from_dev -> RadiotapDecap@45
    -> FilterPhyErr@46
    -> Align@click_align@76
    -> Classifier@47
    -> tx_filter_2
    -> WifiDupeFilter@49
    -> WifiDecap@50
    -> HostEtherFilter@51
    -> Paint@52
    -> cl
    -> WINGCheckHeader@36
    -> wr/ncl
    -> wr/forwarder
    -> wr/dt
    -> WINGSetHeader@16;
sniff_dev_1/from_dev -> RadiotapDecap@37
    -> FilterPhyErr@38
    -> Align@click_align@75
    -> Classifier@39
    -> tx_filter_1
    -> WifiDupeFilter@41
    -> WifiDecap@42
    -> HostEtherFilter@43
    -> Paint@44
    -> cl;
sl_split/cl [1] -> FullNoteQueue@32
    -> WifiEncap@33
    -> outgoing_2
    -> SetTXPower@30
    -> RadiotapEncap@31
    -> sniff_dev_2/to_dev;
wr/es/ps [1] -> wr/es/es_2
    -> wr/SetTimestamp@14;
wr/gw_responder -> wr/outgoing
    -> WINGSetHeader@15;
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
