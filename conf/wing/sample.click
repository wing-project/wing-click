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
lt :: LinkTableMulti(IP 6.35.170.123, IFACES " 1 2", BETA 80, DEBUG false);
# 8 "/tmp/wing-mesh.click"
metric :: WINGETTMetric(LT lt);
# 9 "/tmp/wing-mesh.click"
arp :: ARPTableMulti;
# 169 "/tmp/wing-mesh.click"
control :: ControlSocket("TCP", 7777);
# 189 "/tmp/wing-mesh.click"
dyn :: DynGW(DEVNAME wing-mesh, SEL wr/gw);
# 196 "/tmp/wing-mesh.click"
WINGSetHeader@16 :: WINGSetHeader;
# 196 "/tmp/wing-mesh.click"
WINGSetHeader@17 :: WINGSetHeader;
# 203 "/tmp/wing-mesh.click"
outgoing_1 :: PrioSched;
# 204 "/tmp/wing-mesh.click"
SetTXPower@22 :: SetTXPower(POWER 60);
# 205 "/tmp/wing-mesh.click"
RadiotapEncap@23 :: RadiotapEncap;
# 208 "/tmp/wing-mesh.click"
FullNoteQueue@24 :: FullNoteQueue;
# 208 "/tmp/wing-mesh.click"
WifiEncap@25 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 209 "/tmp/wing-mesh.click"
WifiEncap@26 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 214 "/tmp/wing-mesh.click"
outgoing_2 :: PrioSched;
# 215 "/tmp/wing-mesh.click"
SetTXPower@31 :: SetTXPower(POWER 60);
# 216 "/tmp/wing-mesh.click"
RadiotapEncap@32 :: RadiotapEncap;
# 219 "/tmp/wing-mesh.click"
FullNoteQueue@33 :: FullNoteQueue;
# 219 "/tmp/wing-mesh.click"
WifiEncap@34 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 220 "/tmp/wing-mesh.click"
WifiEncap@35 :: WifiEncap(0x0, 00:00:00:00:00:00);
# 222 "/tmp/wing-mesh.click"
cl :: Classifier(12/06AA);
# 222 "/tmp/wing-mesh.click"
WINGCheckHeader@37 :: WINGCheckHeader;
# 227 "/tmp/wing-mesh.click"
RadiotapDecap@38 :: RadiotapDecap;
# 228 "/tmp/wing-mesh.click"
FilterPhyErr@39 :: FilterPhyErr;
# 229 "/tmp/wing-mesh.click"
Classifier@40 :: Classifier(0/08%0c);
# 229 "/tmp/wing-mesh.click"
tx_filter_1 :: FilterTX;
# 231 "/tmp/wing-mesh.click"
WifiDupeFilter@42 :: WifiDupeFilter;
# 232 "/tmp/wing-mesh.click"
WifiDecap@43 :: WifiDecap;
# 233 "/tmp/wing-mesh.click"
HostEtherFilter@44 :: HostEtherFilter(06:0C:42:23:AA:7B, DROP_OTHER true, DROP_OWN true);
# 234 "/tmp/wing-mesh.click"
Paint@45 :: Paint(0);
# 237 "/tmp/wing-mesh.click"
Discard@46 :: Discard;
# 240 "/tmp/wing-mesh.click"
RadiotapDecap@47 :: RadiotapDecap;
# 241 "/tmp/wing-mesh.click"
FilterPhyErr@48 :: FilterPhyErr;
# 242 "/tmp/wing-mesh.click"
Classifier@49 :: Classifier(0/08%0c);
# 242 "/tmp/wing-mesh.click"
tx_filter_2 :: FilterTX;
# 244 "/tmp/wing-mesh.click"
WifiDupeFilter@51 :: WifiDupeFilter;
# 245 "/tmp/wing-mesh.click"
WifiDecap@52 :: WifiDecap;
# 246 "/tmp/wing-mesh.click"
HostEtherFilter@53 :: HostEtherFilter(00:0C:42:23:AA:70, DROP_OTHER true, DROP_OWN true);
# 247 "/tmp/wing-mesh.click"
Paint@54 :: Paint(1);
# 250 "/tmp/wing-mesh.click"
Discard@55 :: Discard;
# 44 "/tmp/wing-mesh.click"
wr/outgoing :: SetTXRate(12);
# 19 "/tmp/wing-mesh.click"
wr/es/ps :: PaintSwitch;
# 20 "/tmp/wing-mesh.click"
wr/es/es_1 :: WINGLinkStat(DEV dev_1,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  METRIC metric,
                                  LT lt,
                                  ARP arp,
                                  DEBUG false);
# 29 "/tmp/wing-mesh.click"
wr/es/es_2 :: WINGLinkStat(DEV dev_2,
                                  PERIOD 10000,
                                  TAU 100000,
                                  PROBES "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500",
                                  METRIC metric,
                                  LT lt,
                                  ARP arp,
                                  DEBUG false);
# 51 "/tmp/wing-mesh.click"
wr/gw :: WINGGatewaySelector(IP 6.35.170.123, 
                         PERIOD 15000,
                         EXPIRE 30000,
                         LT lt, 
                         ARP arp,
                         DEBUG false);
# 59 "/tmp/wing-mesh.click"
wr/set_gateway :: WINGSetGateway(SEL gw);
# 62 "/tmp/wing-mesh.click"
wr/forwarder :: WINGForwarder(IP 6.35.170.123, ARP arp);
# 65 "/tmp/wing-mesh.click"
wr/querier :: WINGQuerier(IP 6.35.170.123,
                       QUERY_WAIT 5,
                       TIME_BEFORE_SWITCH 5,
                       LT lt, 
                       ARP arp,
                       DEBUG false);
# 73 "/tmp/wing-mesh.click"
wr/query_forwarder :: WINGMetricFlood(IP 6.35.170.123, 
                                  LT lt, 
                                  ARP arp,
                                  DEBUG false);
# 79 "/tmp/wing-mesh.click"
wr/query_responder :: WINGQueryResponder(IP 6.35.170.123,
                                     LT lt, 
                                     ARP arp,
                                     DEBUG false);
# 85 "/tmp/wing-mesh.click"
wr/gw_responder :: WINGGatewayResponder(PERIOD 15000,
                                 SEL gw, 
                                 LT lt, 
                                 MF query_forwarder);
# 101 "/tmp/wing-mesh.click"
wr/host_cl :: IPClassifier(dst net 6.35.170.123 mask 255.0.0.0, -);
lb :: WINGLocalBroadCast();
# 110 "/tmp/wing-mesh.click"
wr/dt :: DecIPTTL;
# 115 "/tmp/wing-mesh.click"
wr/ICMPError@12 :: ICMPError(6.35.170.123, timeexceeded, 0);
# 120 "/tmp/wing-mesh.click"
wr/SetTimestamp@13 :: SetTimestamp;
# 123 "/tmp/wing-mesh.click"
wr/WINGStripHeader@14 :: WINGStripHeader;
# 125 "/tmp/wing-mesh.click"
wr/check_ip :: CheckIPHeader(CHECKSUM false);
# 126 "/tmp/wing-mesh.click"
wr/from_gw_cl :: IPClassifier(src net 6.35.170.123 mask 255.0.0.0, -);
# 134 "/tmp/wing-mesh.click"
wr/ncl :: Classifier(15/01 , // forwarder
                       15/02 , // queries
                       15/03 , // replies
                       15/04 , // es
                       15/05 , // gw
                       15/06 , // bcast
                      );
# 148 "/tmp/wing-mesh.click"
wr/Strip@18 :: Strip(22);
# 175 "/tmp/wing-mesh.click"
linux_ip_host/KernelTun@1 :: KernelTun(6.35.170.123/255.0.0.0, MTU 1500, DEVNAME wing-mesh);
# 176 "/tmp/wing-mesh.click"
linux_ip_host/CheckIPHeader@2 :: CheckIPHeader(CHECKSUM false);
# 12 "/tmp/wing-mesh.click"
rc_split/cl :: Classifier(6/060C4223AA7B, 6/000C4223AA70);
# 12 "/tmp/wing-mesh.click"
sl_split/cl :: Classifier(6/060C4223AA7B, 6/000C4223AA70);
# 182 "/tmp/wing-mesh.click"
sniff_dev_1/from_dev :: FromDevice(ath1, PROMISC false, OUTBOUND true, SNIFFER false);
# 184 "/tmp/wing-mesh.click"
sniff_dev_1/to_dev :: ToDevice(ath1);
# 155 "/tmp/wing-mesh.click"
rc_1/rate_control :: Minstrel(OFFSET 4, RT rates_1);
# 165 "/tmp/wing-mesh.click"
ls_1/FullNoteQueue@1 :: FullNoteQueue(10);
# 182 "/tmp/wing-mesh.click"
sniff_dev_2/from_dev :: FromDevice(ath2, PROMISC false, OUTBOUND true, SNIFFER false);
# 184 "/tmp/wing-mesh.click"
sniff_dev_2/to_dev :: ToDevice(ath2);
# 155 "/tmp/wing-mesh.click"
rc_2/rate_control :: Minstrel(OFFSET 4, RT rates_2);
# 165 "/tmp/wing-mesh.click"
ls_2/FullNoteQueue@1 :: FullNoteQueue(10);
# 0 "<click-align>"
Align@click_align@78 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@79 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@80 :: Align(4, 0);
# 0 "<click-align>"
AlignmentInfo@click_align@81 :: AlignmentInfo(cl  4 2,
  Classifier@40  4 0,
  Classifier@49  4 0,
  wr/check_ip  4 0,
  wr/ncl  4 2,
  linux_ip_host/CheckIPHeader@2  4 0,
  rc_split/cl  4 2,
  sl_split/cl  4 0);
# 210 ""
rc_split/cl [1] -> ls_2/FullNoteQueue@1
    -> WifiEncap@35
    -> rc_2/rate_control
    -> [1] outgoing_2;
tx_filter_2 [1] -> [1] rc_2/rate_control;
tx_filter_1 [1] -> [1] rc_1/rate_control;
wr/set_gateway [1] -> linux_ip_host/KernelTun@1
    -> linux_ip_host/CheckIPHeader@2
    -> wr/host_cl
    -> wr/querier
    -> WINGSetHeader@17
    -> rc_split/cl
    -> ls_1/FullNoteQueue@1
    -> WifiEncap@26
    -> rc_1/rate_control
    -> [1] outgoing_1;
wr/ncl [3] -> wr/es/ps
    -> wr/es/es_1
    -> wr/SetTimestamp@13
    -> WINGSetHeader@16
    -> Align@click_align@80
    -> sl_split/cl
    -> FullNoteQueue@24
    -> WifiEncap@25
    -> outgoing_1
    -> SetTXPower@22
    -> RadiotapEncap@23
    -> sniff_dev_1/to_dev;
rc_2/rate_control [1] -> Discard@55;
sniff_dev_2/from_dev -> RadiotapDecap@47
    -> FilterPhyErr@48
    -> Align@click_align@79
    -> Classifier@49
    -> tx_filter_2
    -> WifiDupeFilter@51
    -> WifiDecap@52
    -> HostEtherFilter@53
    -> Paint@54
    -> cl
    -> WINGCheckHeader@37
    -> wr/ncl
    -> wr/forwarder
    -> wr/dt
    -> WINGSetHeader@17;
rc_1/rate_control [1] -> Discard@46;
sniff_dev_1/from_dev -> RadiotapDecap@38
    -> FilterPhyErr@39
    -> Align@click_align@78
    -> Classifier@40
    -> tx_filter_1
    -> WifiDupeFilter@42
    -> WifiDecap@43
    -> HostEtherFilter@44
    -> Paint@45
    -> cl;
sl_split/cl [1] -> FullNoteQueue@33
    -> WifiEncap@34
    -> outgoing_2
    -> SetTXPower@31
    -> RadiotapEncap@32
    -> sniff_dev_2/to_dev;
wr/es/ps [1] -> wr/es/es_2
    -> wr/SetTimestamp@13;
wr/gw_responder -> wr/outgoing
    -> WINGSetHeader@16;
wr/query_forwarder [1] -> wr/query_responder
    -> wr/outgoing;
wr/host_cl [1] -> wr/set_gateway
    -> wr/querier;
wr/dt [1] -> wr/ICMPError@12
    -> wr/querier;
wr/querier [1] -> wr/query_forwarder
    -> wr/outgoing;
wr/ncl [5] -> wr/Strip@18
    -> wr/check_ip
    -> wr/from_gw_cl
    -> linux_ip_host/KernelTun@1;
wr/forwarder [1] -> wr/WINGStripHeader@14
    -> wr/check_ip;
wr/ncl [4] -> wr/gw
    -> wr/outgoing;
wr/from_gw_cl [1] -> [1] wr/set_gateway;
wr/ncl [2] -> [1] wr/query_responder;
wr/ncl [1] -> [1] wr/query_forwarder;
