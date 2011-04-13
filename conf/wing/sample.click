# 1 "/tmp/wing-mesh.click"
rates_1 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 06:0C:42:23:AA:7B 12 18 24 36 48 72 96 108);
# 2 "/tmp/wing-mesh.click"
channels_1 :: AvailableChannels(DEFAULT 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462, 06:0C:42:23:AA:7B 2412 2417 2422 2427 2432 2437 2442 2447 2452 2457 2462);
# 3 "/tmp/wing-mesh.click"
dev_1 :: DevInfo(ETH 06:0C:42:23:AA:7B, IFID 1, CHANNEL 2412, CHANNELS channels_1, RATES rates_1);
# 4 "/tmp/wing-mesh.click"
rates_2 :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, 00:0C:42:23:AA:70 12 18 24 36 48 72 96 108);
# 5 "/tmp/wing-mesh.click"
channels_2 :: AvailableChannels(DEFAULT 5180 5200 5220 5240 5260 5280 5300 5320, 00:0C:42:23:AA:70 5180 5200 5220 5240 5260 5280 5300 5320);
# 6 "/tmp/wing-mesh.click"
dev_2 :: DevInfo(ETH 00:0C:42:23:AA:70, IFID 2, CHANNEL 5180, CHANNELS channels_2, RATES rates_2);
# 7 "/tmp/wing-mesh.click"
lt :: LinkTableMulti(IP 6.35.170.123, IFACES " 1 2", BETA 80);
# 8 "/tmp/wing-mesh.click"
metric :: WINGETTMetric(LT lt);
# 9 "/tmp/wing-mesh.click"
arp :: ARPTableMulti;
# 181 "/tmp/wing-mesh.click"
control :: ControlSocket("TCP", 7777);
# 183 "/tmp/wing-mesh.click"
err :: WifiDupeFilter;
# 184 "/tmp/wing-mesh.click"
WifiDecap@12 :: WifiDecap;
# 185 "/tmp/wing-mesh.click"
WINGCheckHeader@13 :: WINGCheckHeader;
# 186 "/tmp/wing-mesh.click"
# 187 "/tmp/wing-mesh.click"
Discard@15 :: Discard;
# 212 "/tmp/wing-mesh.click"
WINGSetHeader@20 :: WINGSetHeader;
# 212 "/tmp/wing-mesh.click"
WINGSetHeader@21 :: WINGSetHeader;
# 212 "/tmp/wing-mesh.click"
Print@22 :: Print(X);
# 219 "/tmp/wing-mesh.click"
outgoing_1 :: PrioSched;
# 220 "/tmp/wing-mesh.click"
SetTXPower@27 :: SetTXPower(POWER 60);
# 221 "/tmp/wing-mesh.click"
RadiotapEncap@28 :: RadiotapEncap;
# 224 "/tmp/wing-mesh.click"
FullNoteQueue@29 :: FullNoteQueue;
# 224 "/tmp/wing-mesh.click"
WifiEncap@30 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 225 "/tmp/wing-mesh.click"
WifiEncap@31 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 225 "/tmp/wing-mesh.click"
Print@32 :: Print(OU);
# 230 "/tmp/wing-mesh.click"
outgoing_2 :: PrioSched;
# 231 "/tmp/wing-mesh.click"
SetTXPower@37 :: SetTXPower(POWER 60);
# 232 "/tmp/wing-mesh.click"
RadiotapEncap@38 :: RadiotapEncap;
# 235 "/tmp/wing-mesh.click"
FullNoteQueue@39 :: FullNoteQueue;
# 235 "/tmp/wing-mesh.click"
WifiEncap@40 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 236 "/tmp/wing-mesh.click"
WifiEncap@41 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 236 "/tmp/wing-mesh.click"
Print@42 :: Print(OU);
# 238 "/tmp/wing-mesh.click"
cl :: Classifier(12/06AA);
# 238 "/tmp/wing-mesh.click"
WINGCheckHeader@44 :: WINGCheckHeader;
# 243 "/tmp/wing-mesh.click"
RadiotapDecap@45 :: RadiotapDecap;
# 244 "/tmp/wing-mesh.click"
FilterPhyErr@46 :: FilterPhyErr;
# 245 "/tmp/wing-mesh.click"
Classifier@47 :: Classifier(0/08%0c);
# 245 "/tmp/wing-mesh.click"
tx_filter_1 :: FilterTX;
# 247 "/tmp/wing-mesh.click"
WifiDupeFilter@49 :: WifiDupeFilter;
# 248 "/tmp/wing-mesh.click"
WifiDecap@50 :: WifiDecap;
# 249 "/tmp/wing-mesh.click"
HostEtherFilter@51 :: HostEtherFilter(06:0C:42:23:AA:7B, DROP_OTHER true, DROP_OWN true);
# 250 "/tmp/wing-mesh.click"
Paint@52 :: Paint(0);
# 256 "/tmp/wing-mesh.click"
RadiotapDecap@53 :: RadiotapDecap;
# 257 "/tmp/wing-mesh.click"
FilterPhyErr@54 :: FilterPhyErr;
# 258 "/tmp/wing-mesh.click"
Classifier@55 :: Classifier(0/08%0c);
# 258 "/tmp/wing-mesh.click"
tx_filter_2 :: FilterTX;
# 260 "/tmp/wing-mesh.click"
WifiDupeFilter@57 :: WifiDupeFilter;
# 261 "/tmp/wing-mesh.click"
WifiDecap@58 :: WifiDecap;
# 262 "/tmp/wing-mesh.click"
HostEtherFilter@59 :: HostEtherFilter(00:0C:42:23:AA:70, DROP_OTHER true, DROP_OWN true);
# 263 "/tmp/wing-mesh.click"
Paint@60 :: Paint(1);
# 46 "/tmp/wing-mesh.click"
wr/outgoing :: SetTXRate(12);
# 19 "/tmp/wing-mesh.click"
wr/es/ps :: PaintSwitch;
# 20 "/tmp/wing-mesh.click"
wr/es/es_1 :: WINGLinkStat(IP 6.35.170.123, 
                                  DEV dev_1,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  ARP arp,
                                  LT lt,
                                  METRIC metric,
                                  DEBUG false);
# 30 "/tmp/wing-mesh.click"
wr/es/es_2 :: WINGLinkStat(IP 6.35.170.123, 
                                  DEV dev_2,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  ARP arp,
                                  LT lt,
                                  METRIC metric,
                                  DEBUG false);
# 53 "/tmp/wing-mesh.click"
wr/gw :: WINGGatewaySelector(IP 6.35.170.123, 
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
wr/forwarder :: WINGForwarder(IP 6.35.170.123, 
                          ARP arp,
                          DEBUG false);
# 72 "/tmp/wing-mesh.click"
wr/querier :: WINGQuerier(QUERY_WAIT 5,
                         TIME_BEFORE_SWITCH 5,
			 IP 6.35.170.123,
			 ARP arp,
                         LT lt, 
                         DEBUG false);
# 80 "/tmp/wing-mesh.click"
wr/query_forwarder :: WINGMetricFlood(IP 6.35.170.123, 
                                  LT lt, 
                                  ARP arp,
                                  DEBUG false);
# 86 "/tmp/wing-mesh.click"
wr/query_responder :: WINGQueryResponder(IP 6.35.170.123,
                                     LT lt, 
                                     ARP arp,
                                     DEBUG false);
# 92 "/tmp/wing-mesh.click"
wr/gw_responder :: WINGGatewayResponder(
                                 PERIOD 15000,
                                 SEL gw, 
                                 LT lt, 
                                 RESPONDER query_responder,
                                 DEBUG false);
# 110 "/tmp/wing-mesh.click"
wr/host_cl :: IPClassifier(dst net 6.35.170.123 mask 255.0.0.0, -);
# 122 "/tmp/wing-mesh.click"
wr/dt :: DecIPTTL;
# 127 "/tmp/wing-mesh.click"
wr/ICMPError@13 :: ICMPError(6.35.170.123, timeexceeded, 0);
# 132 "/tmp/wing-mesh.click"
wr/SetTimestamp@14 :: SetTimestamp;
# 135 "/tmp/wing-mesh.click"
wr/WINGStripHeader@15 :: WINGStripHeader;
# 137 "/tmp/wing-mesh.click"
wr/CheckIPHeader@16 :: CheckIPHeader(CHECKSUM false);
# 138 "/tmp/wing-mesh.click"
wr/from_gw_cl :: IPClassifier(src net 6.35.170.123 mask 255.0.0.0, -);
# 146 "/tmp/wing-mesh.click"
wr/ncl :: Classifier(15/01 , // forwarder
                       15/02 , // queries
                       15/03 , // replies
                       15/04 , // es
                       15/05 , // gw
		       15/06 , // bcast
                      );
# 160 "/tmp/wing-mesh.click"
wr/Strip@19 :: Strip(22);
# 160 "/tmp/wing-mesh.click"
wr/Print@20 :: Print;
# 160 "/tmp/wing-mesh.click"
wr/Discard@21 :: Discard;
# 193 "/tmp/wing-mesh.click"
linux_ip_host/KernelTun@1 :: KernelTun(6.35.170.123/255.0.0.0, MTU 1500, DEVNAME wing-mesh);
# 194 "/tmp/wing-mesh.click"
linux_ip_host/CheckIPHeader@2 :: CheckIPHeader(CHECKSUM false);
# 12 "/tmp/wing-mesh.click"
rc_split/cl :: Classifier(6/060C4223AA7B, 6/000C4223AA70);
# 12 "/tmp/wing-mesh.click"
sl_split/cl :: Classifier(6/060C4223AA7B, 6/000C4223AA70);
# 200 "/tmp/wing-mesh.click"
sniff_dev_1/from_dev :: FromDevice(ath1, PROMISC false, OUTBOUND true, SNIFFER false);
# 202 "/tmp/wing-mesh.click"
sniff_dev_1/to_dev :: ToDevice(ath1);
# 167 "/tmp/wing-mesh.click"
rc_1/rate_control :: Minstrel(OFFSET 4, RT rates_1);
# 177 "/tmp/wing-mesh.click"
ls_1/FullNoteQueue@1 :: FullNoteQueue(10);
# 200 "/tmp/wing-mesh.click"
sniff_dev_2/from_dev :: FromDevice(ath2, PROMISC false, OUTBOUND true, SNIFFER false);
# 202 "/tmp/wing-mesh.click"
sniff_dev_2/to_dev :: ToDevice(ath2);
# 167 "/tmp/wing-mesh.click"
rc_2/rate_control :: Minstrel(OFFSET 4, RT rates_2);
# 177 "/tmp/wing-mesh.click"
ls_2/FullNoteQueue@1 :: FullNoteQueue(10);
# 0 "<click-align>"
Align@click_align@86 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@87 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@88 :: Align(4, 0);
# 0 "<click-align>"
AlignmentInfo@click_align@89 :: AlignmentInfo(cl  4 2,
  Classifier@47  4 0,
  Classifier@55  4 0,
  wr/CheckIPHeader@16  4 0,
  wr/ncl  4 2,
  linux_ip_host/CheckIPHeader@2  4 0,
  rc_split/cl  4 2,
  sl_split/cl  4 0);
# 232 ""
rc_split/cl [1] -> ls_2/FullNoteQueue@1
    -> WifiEncap@41
    -> rc_2/rate_control
    -> Print@42
    -> [1] outgoing_2;
tx_filter_2 [1] -> [1] rc_2/rate_control;
tx_filter_1 [1] -> [1] rc_1/rate_control;
wr/track_flows [1] -> linux_ip_host/KernelTun@1
    -> linux_ip_host/CheckIPHeader@2
    -> wr/host_cl
    -> wr/querier
    -> WINGSetHeader@21
    -> Print@22
    -> rc_split/cl
    -> ls_1/FullNoteQueue@1
    -> WifiEncap@31
    -> rc_1/rate_control
    -> Print@32
    -> [1] outgoing_1;
wr/ncl [3] -> wr/es/ps
    -> wr/es/es_1
    -> wr/SetTimestamp@14
    -> WINGSetHeader@20
    -> Align@click_align@88
    -> sl_split/cl
    -> FullNoteQueue@29
    -> WifiEncap@30
    -> outgoing_1
    -> SetTXPower@27
    -> RadiotapEncap@28
    -> sniff_dev_1/to_dev;
rc_2/rate_control [1] -> err
    -> WifiDecap@12
    -> WINGCheckHeader@13
    -> WINGRouteResponder@14
    -> Discard@15;
sniff_dev_2/from_dev -> RadiotapDecap@53
    -> FilterPhyErr@54
    -> Align@click_align@87
    -> Classifier@55
    -> tx_filter_2
    -> WifiDupeFilter@57
    -> WifiDecap@58
    -> HostEtherFilter@59
    -> Paint@60
    -> cl
    -> WINGCheckHeader@44
    -> wr/ncl
    -> wr/forwarder
    -> wr/dt
    -> WINGSetHeader@21;
rc_1/rate_control [1] -> err;
sniff_dev_1/from_dev -> RadiotapDecap@45
    -> FilterPhyErr@46
    -> Align@click_align@86
    -> Classifier@47
    -> tx_filter_1
    -> WifiDupeFilter@49
    -> WifiDecap@50
    -> HostEtherFilter@51
    -> Paint@52
    -> cl;
sl_split/cl [1] -> FullNoteQueue@39
    -> WifiEncap@40
    -> outgoing_2
    -> SetTXPower@37
    -> RadiotapEncap@38
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
wr/set_gateway [1] -> wr/querier;
wr/dt [1] -> wr/ICMPError@13
    -> wr/querier;
wr/querier [1] -> wr/query_forwarder
    -> wr/outgoing;
wr/forwarder [1] -> wr/WINGStripHeader@15
    -> wr/CheckIPHeader@16
    -> wr/from_gw_cl
    -> linux_ip_host/KernelTun@1;
wr/ncl [5] -> wr/Strip@19
    -> wr/Print@20
    -> wr/Discard@21;
wr/from_gw_cl [1] -> [1] wr/track_flows;
wr/ncl [4] -> wr/gw
    -> wr/outgoing;
wr/ncl [2] -> [1] wr/query_responder;
wr/ncl [1] -> [1] wr/query_forwarder;
