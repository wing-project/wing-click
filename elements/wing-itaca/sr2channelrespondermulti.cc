/*
 * SR2ChannelResponder.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussrqueryresponders Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <elements/wifi/wirelessinfo.hh>
#include <elements/standard/counter.hh>
#include "arptablemulti.hh"
#include "availableinterfaces.hh"
#include "sr2packetmulti.hh"
#include "sr2linktablemulti.hh"
#include "sr2channelassignermulti.hh"
#include "sr2channelselectormulti.hh"
#include "sr2channelrespondermulti.hh"
#include "sr2countermulti.hh"
CLICK_DECLS

SR2ChannelResponderMulti::SR2ChannelResponderMulti()
  :  _ip(),
     _et(0),
     _arp_table(0),
     _link_table(0),
     _if_table(0),
     _timer(this)
{
}

SR2ChannelResponderMulti::~SR2ChannelResponderMulti()
{
}

int
SR2ChannelResponderMulti::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  ret = cp_va_kparse(conf, this, errh,
		     "ETHTYPE", 0, cpUnsigned, &_et,
         "ETH", 0, cpEtherAddress, &_eth,
		     "IP", 0, cpIPAddress, &_ip,
		     "LT", 0, cpElement, &_link_table,
		     "IT", 0, cpElement, &_if_table,
		     "ARP", 0, cpElement, &_arp_table,
         "WINFO", 0, cpElement, &_winfo,
         "CHSTR", 0, cpString, &_chstr,
         "PERIOD", 0, cpUnsigned, &_period,
         "CHANPERIOD", 0, cpUnsigned, &_chan_period,
		     "CHSEL", 0, cpElement, &_ch_sel,
		     "COUNT", 0, cpElement, &_pkcounter,
		     "DEBUG", 0, cpBool, &_debug,
		     cpEnd);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_period) 
    return errh->error("PERIOD not specified");
  if (!_chan_period) 
    return errh->error("PERIOD not specified");
  if (!_eth)
    return errh->error("ETH not specified or invalid");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_link_table) 
    return errh->error("LT not specified");
  if (!_arp_table) 
    return errh->error("ARPTableMulti not specified");
  if (!_link_table) 
    return errh->error("SR2LinkTableMulti not specified");
  if (!_chstr) 
    return errh->error("CHSTR not specified");
  if (!_period) 
    return errh->error("PERIOD not specified");
  if (!_chan_period) 
    return errh->error("CHANPERIOD not specified");
  if (!_winfo || _winfo->cast("WirelessInfo") == 0)
    return errh->error("WirelessInfo element is not provided or not a WirelessInfo");
  if (!_ch_sel) 
    return errh->error("SR2ChannelSelectorMulti not specified");
  if (_arp_table->cast("ARPTableMulti") == 0) 
    return errh->error("ARPTableMulti element is not a ARPTableMulti");
  if (_ch_sel->cast("SR2ChannelSelectorMulti") == 0) 
    return errh->error("SR2ChannelSelectorMulti element is not a SR2ChannelSelectorMulti");
  if (_link_table->cast("SR2LinkTableMulti") == 0) 
    return errh->error("SR2LinkTableMulti element is not a SR2LinkTableMulti");
  if (_if_table && _if_table->cast("AvailableInterfaces") == 0) 
    return errh->error("AvailableInterfaces element is not an AvailableInterfaces");

  return ret;
}

int
SR2ChannelResponderMulti::initialize (ErrorHandler *)
{
  _original_channel = 0;
  _total_channels = 0;
  _channel = NULL;
  _sniffing = false;
  _timer.initialize(this);
  Timestamp delay = Timestamp::make_msec(_period / 10);
	_timer.schedule_at(Timestamp::now() + delay);
	return 0;
}

void
SR2ChannelResponderMulti::start_sniff(){
  
  _original_channel = _winfo->_channel;
	int channels = 0;
  
  if (_original_channel <= 11) {
    // 802.11b/g
		channels = 11;
		_version = 1;
		int ch[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
		_channel = new int[channels];
		for(int j=0; j<channels; j++){
    	_channel[j] = ch[j];
		}
    _total_channels = 11;
  }
  
  if (_original_channel >= 36) {
    //802.11a
		//channels = 24;
		channels = 12;
		_version = 2;
		//int ch[24] = {36, 40, 44, 48, 52, 56, 58, 60, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165};
		int ch[12] = {36, 40, 44, 48, 52, 56, 60, 149, 153, 157, 161, 165};
		_channel = new int[channels];
		for(int j=0; j<channels; j++){
    	_channel[j] = ch[j];
		}
    //_total_channels = 24;
		_total_channels = 12;
  }
}

void
SR2ChannelResponderMulti::run_timer(Timer *)
{

  int channel_to_switch;
  int iface = _if_table->lookup_id(_eth);
	_winfo->_channel = iface % 256;
	_original_channel = iface % 256;
	SR2ChannelAssignment ch_ass;
	ch_ass._node = NodeAddress (_ip, iface);
	ch_ass._new_iface = 0;

  if (!_sniffing){
		 
			for (int i=0; i<3; i++){
				_ch_sel->send_change_warning(true, true, ch_ass);
			}
     _if_table->set_unavailable(iface);
     _pkcounter->set_discard(true);
     _sniffing = true;
     start_sniff();
     _channel_index = 0;
     channel_to_switch = _channel[_channel_index];
   } else {
     // Reading counter
     int linkload = _pkcounter->read_busy_time();;
     _ctable.update(_channel[_channel_index], linkload);
     click_chatter("%{element}: Update channel info, channel %d value %d\n",
				      this,
				      _channel[_channel_index],
				      linkload);
     _channel_index++;
     channel_to_switch = _channel[_channel_index];

     if (_channel_index == _total_channels){
	
       int switch_to = _if_table->check_channel_change(iface);

       if (switch_to != 0){
	      _sniffing = false;
        _pkcounter->set_discard(false);
        _if_table->set_available(iface);
				for (int i=0; i<3; i++){
					_ch_sel->send_change_warning(false, true, ch_ass);
				}
				if (_debug){
		     click_chatter("%{element}: Channel switch occurred during scan, switching now to channel %d\n",
					      this,
					      switch_to);
				}

				ch_ass._new_iface = switch_to;

				_ch_sel->switch_channel(true,ch_ass);

				_if_table->set_channel_change(switch_to,0);

				unsigned max_jitter = _period / 10;
       	unsigned j = click_random(0, 2 * max_jitter);
        Timestamp delay = Timestamp::make_msec(_period + j - max_jitter + 100000);
       	_timer.schedule_at(Timestamp::now() + delay);

				return;
				
       } else {
	
         channel_to_switch = _original_channel;

       }
     }
   }

  StringAccum switchcmd;

  switchcmd << "iwconfig " << _chstr << " channel " << channel_to_switch;
 
  int switch_error = system (switchcmd.c_str());

  if (switch_error){
      click_chatter("%{element}: channel switching failed using: %s\n",
		      this,
		      switchcmd.c_str());
  } else {
			if (_debug){
				click_chatter("%{element}: Device %s switched on channel %d\n",
						this,
						_chstr.c_str(),
						channel_to_switch);
			}

	}

  _pkcounter->reset();

 	if (_channel_index == _total_channels){
        _sniffing = false;
        _if_table->set_available(iface);
        _pkcounter->set_discard(false);
				for (int i=0; i<3; i++){
					_ch_sel->send_change_warning(false, true, ch_ass);
				}
        
				if (!_ch_sel->is_cas()) {
					for (int i=0; i<3; i++) {
	          send_scandata();
	        }
				} else {
					send_scandata();
				}

        
        unsigned max_jitter = _period / 10;
       	unsigned j = click_random(0, 2 * max_jitter);
        Timestamp delay = Timestamp::make_msec(_period + j - max_jitter + 100000);
       	_timer.schedule_at(Timestamp::now() + delay);
   } else {
         Timestamp delay = Timestamp::make_msec(_chan_period);
       	_timer.schedule_at(Timestamp::now() + delay);
   }

}

void
SR2ChannelResponderMulti::send_scandata()
{

 	if (!_ch_sel->is_cas()) {
 		IPAddress cas = _ch_sel->best_cas();
 		_link_table->dijkstra(false);
 		SR2PathMulti best = _link_table->best_route(cas, true);
 		if (_link_table->valid_route(best)) {
 			int links = best.size() - 1;
 			int len = sr2packetmulti::len_wo_data(links);
 			if (_debug) {
 				click_chatter("%{element}: Reporting Scan Info %s -> %s\n",
 					      this,
 					      _ip.unparse().c_str(),
 					      cas.unparse().c_str());
 			}
      
			int iface = _if_table->lookup_id(_eth);
      NodeAddress node = NodeAddress (_ip, iface);
			int hops = links;
      
			SR2ScanInfo sc_info = SR2ScanInfo (node,_version,_ctable,_link_table->get_neighbors_if(iface),hops);
			
			int data_len = sizeof(sc_info._node)
											+ 4 * sizeof(uint16_t)
											+ sc_info._channel_table.size() * 2 * sizeof(uint32_t)
											+ sc_info._neighbors_table.size() * (sizeof(NodeAddress)+sizeof(uint32_t));
      
 			WritablePacket *p = Packet::make(len + data_len + sizeof(click_ether));
 			if(p == 0)
 				return;
 			click_ether *eh = (click_ether *) p->data();
 			struct sr2packetmulti *pk = (struct sr2packetmulti *) (eh+1);

			int next = index_of(best, _ip);
			if (next < 0 || next >= links) {
				click_chatter("%{element} :: %s :: encap couldn't find %s (%d) in path %s",
							  this,
							  __func__,
				        _ip.unparse().c_str(),
							  next, 
				        path_to_string(best).c_str());
				p->kill();
				return;
			}

			uint16_t version = sc_info._version;
			uint16_t cas_hops = sc_info._cas_hops;
			uint16_t channel_elements = sc_info._channel_table.size();
			uint16_t neighbors_elements = sc_info._neighbors_table.size();		

			size_t position = 0;
			memcpy(p->data()+len+sizeof(click_ether)+position, &node, sizeof(node));
			position = position + sizeof(node);
			memcpy(p->data()+len+sizeof(click_ether)+position, &version, sizeof(uint16_t));
			position = position + sizeof(uint16_t);
			memcpy(p->data()+len+sizeof(click_ether)+position, &cas_hops, sizeof(uint16_t));
			position = position + sizeof(uint16_t);
			memcpy(p->data()+len+sizeof(click_ether)+position, &channel_elements, sizeof(uint16_t));
			position = position + sizeof(uint16_t);
			memcpy(p->data()+len+sizeof(click_ether)+position, &neighbors_elements, sizeof(uint16_t));
			position = position + sizeof(uint16_t);

			for (HashMap<int,int>::const_iterator iter = sc_info._channel_table.begin(); iter.live(); iter++) {
				memcpy(p->data()+len+sizeof(click_ether)+position, &(iter.key()), sizeof(iter.key()));
				position = position + sizeof(iter.key());
				memcpy(p->data()+len+sizeof(click_ether)+position, &(iter.value()), sizeof(iter.value()));
				position = position + sizeof(iter.value());
			}

			for (HashMap<NodeAddress,int>::const_iterator iter = sc_info._neighbors_table.begin(); iter.live(); iter++) {
				memcpy(p->data()+len+sizeof(click_ether)+position, &(iter.key()), sizeof(iter.key()));
				position = position + sizeof(iter.key());
				memcpy(p->data()+len+sizeof(click_ether)+position, &(iter.value()), sizeof(iter.value()));
				position = position + sizeof(iter.value());
			}
 			
 			pk->_version = _sr2_version;
 			pk->_type = SR2_PT_CHSCINFO;
 			pk->unset_flag(~0);
 			pk->set_data_len(data_len);

 			pk->set_seq(0);
 			pk->set_num_links(links);
 			pk->set_next(next);
 			pk->set_qdst(_ip);

 			for (int i = 0; i < links; i++) {
 				pk->set_link(i,
 					     best[i].get_dep(), best[i+1].get_arr(),
 					     _link_table->get_link_metric(best[i].get_dep(), best[i+1].get_arr()),
 					     _link_table->get_link_metric(best[i+1].get_dep(), best[i].get_arr()),
 					     _link_table->get_link_seq(best[i].get_dep(), best[i+1].get_arr()),
 					     _link_table->get_link_age(best[i].get_dep(), best[i+1].get_arr()));
 			}

 			IPAddress next_ip = pk->get_link_node_b(pk->next());
 			uint16_t next_if = pk->get_link_if_b(pk->next());

			EtherAddress eth_dest = _arp_table->lookup_def(NodeAddress(next_ip,next_if));
		 	EtherAddress my_eth = _if_table->lookup_def();

 			eh->ether_type = htons(_et);
 			memcpy(eh->ether_shost, my_eth.data(), 6);
 			memcpy(eh->ether_dhost, eth_dest.data(), 6);
 			output(0).push(p);
 		}	
 	} else {
	
		int iface = _if_table->lookup_id(_eth);
    NodeAddress node = NodeAddress (_ip, iface);
    
		SR2ScanInfo sc_info = SR2ScanInfo (node,_version,_ctable,_link_table->get_neighbors_if(iface),0);
		
		_ch_sel->set_local_scinfo(sc_info);
			
 		}
}

enum {H_DEBUG, H_IP};

String
SR2ChannelResponderMulti::read_handler(Element *e, void *thunk)
{
  SR2ChannelResponderMulti *c = (SR2ChannelResponderMulti *)e;
  switch ((intptr_t)(thunk)) {
  case H_IP:
    return c->_ip.unparse() + "\n";
  default:
    return "<error>\n";
  }
}

int 
SR2ChannelResponderMulti::write_handler(const String &in_s, Element *e, void *vparam,
		     ErrorHandler *errh)
{
  SR2ChannelResponderMulti *d = (SR2ChannelResponderMulti *)e;
  String s = cp_uncomment(in_s);
  switch ((intptr_t)vparam) {
    case H_DEBUG: {
      bool debug;
      if (!cp_bool(s, &debug)) 
        return errh->error("debug parameter must be boolean");
      d->_debug = debug;
      break;
    }
  }
  return 0;
}

void
SR2ChannelResponderMulti::add_handlers()
{
  add_read_handler("ip", read_handler, H_IP);
  add_write_handler("debug", write_handler, H_DEBUG);
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(SR2LinkTableMulti)
EXPORT_ELEMENT(SR2ChannelResponderMulti)
