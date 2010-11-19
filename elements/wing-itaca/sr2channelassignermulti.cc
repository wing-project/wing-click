/*
 * SR2ChannelAssignerMulti.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/ipaddress.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "availableinterfaces.hh"
#include "sr2packetmulti.hh"
#include "sr2linktablemulti.hh"
#include "sr2nodemulti.hh"
#include "sr2channelassignermulti.hh"
#include "sr2channelselectormulti.hh"
CLICK_DECLS

SR2ChannelAssignerMulti::SR2ChannelAssignerMulti()
  :  _ip(),
     _et(0),
     _link_table(0),
     _if_table(0),
     _arp_table(0),
     _timer(this)
{

}

SR2ChannelAssignerMulti::~SR2ChannelAssignerMulti()
{
}

int
SR2ChannelAssignerMulti::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
	_debug = false;
  ret = cp_va_kparse(conf, this, errh,
		     "ETHTYPE", 0, cpUnsignedShort, &_et,
		     "IP", 0, cpIPAddress, &_ip,
		     "LT", 0, cpElement, &_link_table,
		     "IT", 0, cpElement, &_if_table,
		     "ARP", 0, cpElement, &_arp_table,
				 "CHSEL", 0, cpElement, &_ch_sel,
		     "PERIOD", 0, cpUnsigned, &_period,
		     "JITTER", 0, cpUnsigned, &_jitter,
         "VERSION", 0, cpInteger, &_version,
		     "DEBUG", 0, cpBool, &_debug,
		     cpEnd);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_link_table) 
    return errh->error("LT not specified");
  if (!_version) 
    return errh->error("VERSION not specified");
  if (!_ch_sel) 
    return errh->error("SR2ChannelSelectorMulti not specified");
  if (_version!=1 && _version!=2) 
    return errh->error("VERSION is not a valid channel version");
  if (_link_table->cast("SR2LinkTableMulti") == 0) 
    return errh->error("LinkTable element is not a SR2LinkTableMulti");
  if (_if_table && _if_table->cast("AvailableInterfaces") == 0) 
    return errh->error("AvailableInterfaces element is not an AvailableInterfaces");
  if (_arp_table && _arp_table->cast("ARPTableMulti") == 0) 
    return errh->error("ARPTable element is not an ARPtableMulti");
  if (_ch_sel && _ch_sel->cast("SR2ChannelSelectorMulti") == 0) 
    return errh->error("SR2ChannelSelectorMulti element is not a SR2ChannelSelectorMulti");

  return ret;
}

int
SR2ChannelAssignerMulti::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
SR2ChannelAssignerMulti::run_timer (Timer *)
{
  start_assignment();
  unsigned max_jitter = _period / 10;
  unsigned j = click_random(0, 2 * max_jitter);
  Timestamp delay = Timestamp::make_msec(_period + j - max_jitter);
  _timer.schedule_at(Timestamp::now() + delay);
}

static int
delay_sorter(const void *va, const void *vb, void *) {

		NodeDelay *a = (NodeDelay *)va, *b = (NodeDelay *)vb;

		if (a->_delay == b->_delay){
			return 0;
		} else {
			return (a->_delay < b->_delay) ? -1 : 1;
		}

}

Vector<NodeDelay>
SR2ChannelAssignerMulti::smallest_hop_count()
{
	Vector<NodeDelay> nodelist;
	uint32_t min_hop = ~0;
	for (MCGIter iter = _mcgraph.begin(); iter.live(); iter++) {
		NodePair nodep = iter.key();
		MCGLeaf leaf = iter.value();
		if (leaf._hop_count <= min_hop){
			if (leaf._hop_count < min_hop) {
				nodelist.clear();
				min_hop = leaf._hop_count;
			}
			nodelist.push_back(NodeDelay(nodep,leaf._delay));
		}
	}
	return nodelist;
}

Vector<NodeAddress>
SR2ChannelAssignerMulti::remove_neighbor_link(NodeAddress node)
{
	
	Vector<NodePair> remove_link;
	Vector<NodeAddress> change_channel;
	
	for (MCGIter iter = _mcgraph.begin(); iter.live(); iter++) {
		NodePair nodep = iter.key();
		MCGLeaf leaf = iter.value();
		if (nodep._from == node){
			remove_link.push_back(nodep);
			change_channel.push_back(nodep._to);
		}
		if (nodep._to == node){
			remove_link.push_back(nodep);
			change_channel.push_back(nodep._from);
		}
	}

	for (int i=0; i< remove_link.size(); i++){
		NodePair nodep = remove_link[i];
		_mcgraph.erase(nodep);
	}

	return change_channel;

}

void
SR2ChannelAssignerMulti::assign_channel(NodeAddress node, int new_iface){
	
	int * val = _chtable.findp(node);
	
	if (!val){
		_chtable.insert(node,new_iface);
	} else {
		*val = new_iface;
	}
	
}

Vector<NodeDelay>
SR2ChannelAssignerMulti::get_link_from_node(NodeAddress node){
	
	Vector<NodeDelay> links;
	
	for (MCGIter iter = _mcgraph.begin(); iter.live(); iter++){
		NodePair nodep = iter.key();
		MCGLeaf leaf = iter.value();
		if ((nodep._from == node) || (nodep._to == node)){
			NodeDelay noded = NodeDelay(nodep,leaf._delay);
			links.push_back(noded);
		}
	}
	
	return links;
	
}

void
SR2ChannelAssignerMulti::start_assignment()
{

	HashMap <int,SR2ScanInfo> local_scinfo = _ch_sel->get_local_scinfo();
	
	for (HashMap<int,SR2ScanInfo>::const_iterator iter = local_scinfo.begin(); iter.live(); iter++){
		SR2ScanInfo scinfo = iter.value();
		update_scinfo(&scinfo);
	}
	
	// Copying ScanInfo  Table into a temporary table to work on
	NTable ntable_temp;
	ntable_temp.clear();
	
	_busy = true;
	
	for(NIter iter = _ntable.begin(); iter.live(); iter++){
		ntable_temp.insert(iter.key(),iter.value());
	}
	
	// Clean MCG and channel assignment tables
	_mcgraph.clear();
	_chtable.clear();
	
	// Creating MCG
	for(NIter iter = ntable_temp.begin(); iter.live(); iter++){
		SR2ScanInfo scinfo1 = iter.value();
		for(HashMap<NodeAddress,int>::const_iterator iter_n = scinfo1._neighbors_table.begin(); iter_n.live(); iter_n++ ){
			NodeAddress node1 = iter.key();
			NodeAddress node2 = iter_n.key();
			NodePair nodep1 = NodePair(node1,node2);
			NodePair nodep2 = NodePair(node2,node1);
			MCGLeaf * leafnfo1 = _mcgraph.findp(nodep1);
			MCGLeaf * leafnfo2 = _mcgraph.findp(nodep2);
			if ((!leafnfo1) && (!leafnfo2)){
				SR2ScanInfo * scinfo2 = ntable_temp.findp(iter_n.key());
				if (scinfo2){
					// insert link
					MCGLeaf leaf;
					leaf._nodep = nodep1;
					leaf._delay = iter_n.value();
					leaf._hop_from = scinfo1._cas_hops;
					leaf._hop_to = scinfo2->_cas_hops;
					leaf._hop_count = ((leaf._hop_from*100)+(leaf._hop_to*100))/2;
					for (HashMap<int,int>::const_iterator iter_c= scinfo1._channel_table.begin(); iter_c.live(); iter_c++) {
						int channel = iter_c.key();
						int traffic = iter_c.value();
						leaf.update_channel(channel, traffic);
					}
					for (HashMap<int,int>::const_iterator iter_c= scinfo2->_channel_table.begin(); iter_c.live(); iter_c++) {
						int channel = iter_c.key();
						int traffic = iter_c.value();
						leaf.update_channel(channel, traffic);
					}
					for (HashMap<NodeAddress,int>::const_iterator iter_i= scinfo1._neighbors_table.begin(); iter_i.live(); iter_i++) {
						NodeAddress interfering_node = iter_i.key();
						int interfering_node_metric = iter_i.value();
						leaf.update_interfering(NodePair(node1,interfering_node),interfering_node_metric);
					}
					for (HashMap<NodeAddress,int>::const_iterator iter_i= scinfo2->_neighbors_table.begin(); iter_i.live(); iter_i++) {
						NodeAddress interfering_node = iter_i.key();
						int interfering_node_metric = iter_i.value();
						leaf.update_interfering(NodePair(node2,interfering_node),interfering_node_metric);
					}		
					
					_mcgraph.insert(nodep1,leaf);
				}

			}
		}
	}

	_busy = false;
	
	// Running through the graph
	
	while (_mcgraph.size() != 0) {
		
		// Find the nodes with the smallest hop
		Vector<NodeDelay> smallest_hop = smallest_hop_count();
		
		for (int i=0; i<smallest_hop.size(); i++){
			NodeDelay noded = smallest_hop[i];
		}
		
		// Sort this list by increasing delay values
		click_qsort(smallest_hop.begin(), smallest_hop.size(), sizeof(NodeDelay), delay_sorter);
		
		for (int i=0; i<smallest_hop.size(); i++){
			NodeDelay noded = smallest_hop[i];
		}
		
		// Visiting each vertex in smallest_hop
		while (smallest_hop.size() != 0){
	
			NodeDelay noded = smallest_hop[0];
			smallest_hop.pop_front();
			MCGLeaf *leaf = _mcgraph.findp(noded._nodep);

			if (leaf) {
			
				int default_channel = (_if_table->lookup_def_id()) % 256;
				int channel = leaf->best_channel();
			
				while (channel == default_channel){
					channel = leaf->best_channel();
					if (leaf->_channels.size() == 0){
						channel = 1;
						break;
					}
				}
			
				NodeAddress node_a = leaf->_nodep._from;
				NodeAddress node_b = leaf->_nodep._to;
			
				HashMap<NodeAddress,int> neighbor_radios;
			
				if (channel != (node_a._iface % 256)){
					int if_id_a = (node_a._iface / 256);
					int new_if_a = if_id_a * 256 + channel;
					assign_channel(node_a,new_if_a);
				}

				Vector<NodeAddress> n_radios_a =  remove_neighbor_link(node_a);
				for (int j=0; j < n_radios_a.size(); j++){
					NodeAddress node = n_radios_a[j];
					int* nfo = neighbor_radios.findp(node);
					if ((!nfo) && (node != node_a) && (node != node_b)) {
						neighbor_radios.insert(node,0);
					}
				}

				if (channel != (node_b._iface % 256)){
					int if_id_b = (node_b._iface / 256);
					int new_if_b = if_id_b * 256 + channel;
					assign_channel(node_b,new_if_b);
				}

				Vector<NodeAddress> n_radios_b = remove_neighbor_link(node_b);
				for (int j=0; j < n_radios_b.size(); j++){
					NodeAddress node = n_radios_b[j];
					int* nfo = neighbor_radios.findp(node);
					if ((!nfo) && (node != node_a) && (node != node_b)){
						neighbor_radios.insert(node,0);
					}
				}
			
				for (HashMap<NodeAddress,int>::const_iterator iter = neighbor_radios.begin(); iter.live(); iter++){
					NodeAddress node_n = iter.key();
					if (channel != (node_n._iface % 256)){
						int if_id_n = (node_n._iface / 256);
						int new_if_n = if_id_n * 256 + channel;
						assign_channel(node_n,new_if_n);
					}
				}
			
				neighbor_radios.clear();
			
				// Finding farthest side of the link from router
			
				NodeAddress farthest = NodeAddress();
				if (leaf->_hop_from > leaf->_hop_to){
					farthest = leaf->_nodep._from;
				} else {
					farthest = leaf->_nodep._to;
				}
			
				// Creating tail
				Vector<NodeDelay> tail = get_link_from_node(farthest);
			
				for (int i=0; i<tail.size(); i++){
					NodeDelay noded = tail[i];
				}
			
				// Sort this list by increasing delay values
				click_qsort(tail.begin(), tail.size(), sizeof(NodeDelay), delay_sorter);
			
				for (int i=0; i<tail.size(); i++){
					NodeDelay noded = tail[i];
				}
			
				// Put tail into smallest_hop
				for (int i=0; i< tail.size(); i++) {
					NodeDelay noded = tail[i];
					bool found = false;
					for (int j=0; j< smallest_hop.size(); j++) {
						found = true;
					}
					if (!found) {
						smallest_hop.push_back(noded);
					}
				}
			
				tail.clear();

			}
						
		}
		
		smallest_hop.clear();
		
	}

	ntable_temp.clear();

  SR2ChannelAssignment ch_ass;

	for (CHIter iter = _chtable.begin(); iter.live(); iter++){
		SR2ChannelAssignment ch_ass = SR2ChannelAssignment(iter.key(),iter.value());
		if (ch_ass._node._ipaddr == _ip){
			_ch_sel->switch_channel(false, ch_ass);
		} else {
			send (ch_ass);
		}
	}

	_chtable.clear();
	_ntable.clear();
  
}

void
SR2ChannelAssignerMulti::send(SR2ChannelAssignment ch_ass)
{
  
  IPAddress dest_ip = ch_ass._node._ipaddr;
  
	SR2PathMulti best = _link_table->best_route(dest_ip, true);

	if (_link_table->valid_route(best)) {
		int links = best.size() - 1;
		int len = sr2packetmulti::len_wo_data(links);
		if (_debug){
			click_chatter("%{element} :: Sending Channel Assignment: %s-%d >> %d\n",
						this,
						ch_ass._node._ipaddr.unparse().c_str(),
						ch_ass._node._iface,
						ch_ass._new_iface);
		}
    
    int data_len = sizeof (ch_ass);
    
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
		
    memcpy(p->data()+len+sizeof(click_ether), &ch_ass, data_len);
		
		pk->_version = _sr2_version;
		pk->_type = SR2_PT_CHASSIGN;
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
		//EtherAddress eth_dest = _arp_table->lookup(NodeAddress(next_ip,next_if));
		//EtherAddress my_eth = _if_table->lookup_if(best[0].get_dep()._iface);
		
		EtherAddress eth_dest = _arp_table->lookup_def(NodeAddress(next_ip,next_if));
   	EtherAddress my_eth = _if_table->lookup_def();

		eh->ether_type = htons(_et);
		memcpy(eh->ether_shost, my_eth.data(), 6);
		memcpy(eh->ether_dhost, eth_dest.data(), 6);

		output(0).push(p);
	}
}

void SR2ChannelAssignerMulti::update_scinfo(SR2ScanInfo * scinfo)
{
	
	if (!_busy){
	  SR2ScanInfo * sctinfo = _ntable.findp(scinfo->_node);
  
	  if(!sctinfo){
			for (NIter it = _ntable.begin(); it.live(); it++){
	      int iface_id = it.value()._node._iface / 256;
	      int new_iface_id = scinfo->_node._iface / 256;
	  		if ((it.value()._node._ipaddr == scinfo->_node._ipaddr) && (iface_id == new_iface_id)){
	        sctinfo = _ntable.findp(it.key());
					_arp_table->change_if(sctinfo->_node, scinfo->_node._iface);
					_link_table->change_if(sctinfo->_node, scinfo->_node._iface);
	  		}
	  	}
	  }
  
		if(!sctinfo){
	    SR2ScanInfo foo = SR2ScanInfo();
	    _ntable.insert(scinfo->_node,foo);
			sctinfo = _ntable.findp(scinfo->_node);
		}
  
		sctinfo->_node = scinfo->_node;
		sctinfo->_channel_table.clear();
	
	  for (HashMap<int,int>::const_iterator iter = scinfo->_channel_table.begin(); iter.live(); iter++){
	    sctinfo->_channel_table.insert(iter.key(), iter.value());
	  }

		sctinfo->_neighbors_table.clear();

	  for (HashMap<NodeAddress,int>::const_iterator iter = scinfo->_neighbors_table.begin(); iter.live(); iter++){
	    sctinfo->_neighbors_table.insert(iter.key(), iter.value());
	  }

		sctinfo->_version = scinfo->_version;
		sctinfo->_cas_hops = scinfo->_cas_hops;
	
	}

}

bool
SR2ChannelAssignerMulti::update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t metric) {
  if (_link_table && !_link_table->update_link(from, to, seq, 0, metric)) {
    click_chatter("%{element} :: %s :: couldn't update link %s > %d > %s\n",
		  this,
		  __func__,
		  from._ipaddr.unparse().c_str(),
		  metric,
		  to._ipaddr.unparse().c_str());
    return false;
  }
  return true;
}

void
SR2ChannelAssignerMulti::push(int, Packet *p_in)
{
  click_ether *eh = (click_ether *) p_in->data();
  struct sr2packetmulti *pk = (struct sr2packetmulti *) (eh+1);
  if(eh->ether_type != htons(_et)) {
    click_chatter("%{element} :: %s :: %s bad ether_type %04x",
		  this,
		  __func__,
		  _ip.unparse().c_str(),
		  ntohs(eh->ether_type));
    p_in->kill();
    return;
  }
  if (pk->_version != _sr2_version) {
    click_chatter("%{element} :: %s :: bad sr version %d vs %d\n",
		  this,
		  __func__,
		  pk->_version,
		  _sr2_version);
    p_in->kill();
    return;
  }
  if (pk->_type != SR2_PT_CHSCINFO) {
    click_chatter("%{element} :: %s :: back packet type %d",
		  this,
		  __func__,
		  pk->_type);
    p_in->kill();
    return;
  }

	/* I am the ultimate consumer of this packet */
	
  int head_len = pk->len_wo_data(pk->num_links());

	SR2ScanInfo * scinfo = new SR2ScanInfo();

	NodeAddress node;
	uint16_t version;
	uint16_t cas_hops;
	uint16_t channel_elements;
	uint16_t neighbors_elements;

	size_t position = 0;

	memcpy(&node, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(NodeAddress));
	position = position + sizeof(NodeAddress);

	memcpy(&version, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(uint16_t));
	position = position + sizeof(uint16_t);

	memcpy(&cas_hops, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(uint16_t));
	position = position + sizeof(uint16_t);

	memcpy(&channel_elements, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(uint16_t));
	position = position + sizeof(uint16_t);

	memcpy(&neighbors_elements, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(uint16_t));
	position = position + sizeof(uint16_t);

	if (channel_elements > 0) {
		int current_channel = 0;
		int current_value = 0;
		for (int i=0; i < channel_elements; i++){
			memcpy(&current_channel, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(uint32_t));
			position = position + sizeof(uint32_t);
			memcpy(&current_value, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(uint32_t));
			position = position + sizeof(uint32_t);
			scinfo->_channel_table.insert(current_channel, current_value);
		}
	}

	if (neighbors_elements > 0) {
		NodeAddress *current_node = new NodeAddress();
		int current_value = 0;
		for (int i=0; i < neighbors_elements; i++){
			memcpy(current_node, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(NodeAddress));
			position = position + sizeof(NodeAddress);
			memcpy(&current_value, p_in->data()+head_len+sizeof(click_ether)+position, sizeof(uint32_t));
			position = position + sizeof(uint32_t);
			scinfo->_neighbors_table.insert(*current_node, current_value);
		}
	}

	scinfo->_node = node;
	scinfo->_version = version;
	scinfo->_cas_hops = cas_hops;
  
  update_scinfo(scinfo);
  
	return;

}
/*
String
SR2ChannelAssignerMulti::print_cas_stats()
{
    StringAccum sa;
    Timestamp now = Timestamp::now();
    for(CASIter iter = _castable.begin(); iter.live(); iter++) {
      CASInfo nfo = iter.value();
      sa << nfo._ip.unparse().c_str() << " ";
      sa << "seen " << nfo._seen << " ";
      sa << "first_update " << now - nfo._first_update << " ";
      sa << "last_update " << now - nfo._last_update << " ";
      
      SR2PathMulti p = _link_table->best_route(nfo._ip, false);
      int metric = _link_table->get_route_metric(p);
      sa << "current_metric " << metric << "\n";
    }
    
  return sa.take_string();
}
enum { H_IS_CAS, H_CAS_STATS, H_ALLOW, H_ALLOW_ADD, H_ALLOW_DEL, H_ALLOW_CLEAR, H_IGNORE, H_IGNORE_ADD, H_IGNORE_DEL, H_IGNORE_CLEAR};

String
SR2ChannelAssignerMulti::read_handler(Element *e, void *thunk)
{
  SR2ChannelAssignerMulti *f = (SR2ChannelAssignerMulti *)e;
  switch ((uintptr_t) thunk) {
  case H_IS_CAS:
    return String(f->_is_cas) + "\n";
  case H_CAS_STATS:
    return f->print_cas_stats();
  case H_IGNORE: {
    StringAccum sa;
    for (IPIter iter = f->_ignore.begin(); iter.live(); iter++) {
      IPAddress ip = iter.key();
      sa << ip << "\n";
    }
    return sa.take_string();    
  }
  case H_ALLOW: {
    StringAccum sa;
    for (IPIter iter = f->_allow.begin(); iter.live(); iter++) {
      IPAddress ip = iter.key();
      sa << ip << "\n";
    }
    return sa.take_string();    
  }
  default:
    return String();
  }
}

int 
SR2ChannelAssignerMulti::write_handler(const String &in_s, Element *e, void *vparam,
		     ErrorHandler *errh)
{
  SR2ChannelAssignerMulti *f = (SR2ChannelAssignerMulti *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_IS_CAS: {  
      bool b;
      if (!cp_bool(s, &b)) 
        return errh->error("is_cas parameter must be boolean");
      f->_is_cas = b;
      break;
    }
    case H_IGNORE_ADD: {  
      IPAddress ip;
      if (!cp_ip_address(s, &ip)) {
        return errh->error("ignore_add parameter must be IPAddress");
      }
      f->_ignore.insert(ip, ip);
      break;
    }
    case H_IGNORE_DEL: {  
      IPAddress ip;
      if (!cp_ip_address(s, &ip)) {
        return errh->error("ignore_add parameter must be IPAddress");
      }
      f->_ignore.remove(ip);
      break;
    }
    case H_IGNORE_CLEAR: {  
      f->_ignore.clear();
      break;
    }
    case H_ALLOW_ADD: {  
      IPAddress ip;
      if (!cp_ip_address(s, &ip)) {
        return errh->error("ignore_add parameter must be IPAddress");
      }
      f->_allow.insert(ip, ip);
      break;
    }
    case H_ALLOW_DEL: {  
      IPAddress ip;
      if (!cp_ip_address(s, &ip)) {
        return errh->error("ignore_add parameter must be IPAddress");
      }
      f->_allow.remove(ip);
      break;
    }
    case H_ALLOW_CLEAR: {  
      f->_allow.clear();
      break;
    }
  }
  return 0;
}

void
SR2ChannelAssignerMulti::add_handlers()
{
  add_read_handler("is_cas", read_handler, (void *) H_IS_CAS);
  add_read_handler("cas_stats", read_handler, (void *) H_CAS_STATS);
  add_read_handler("ignore", read_handler, (void *) H_IGNORE);
  add_read_handler("allow", read_handler, (void *) H_ALLOW);

  add_write_handler("is_cas", write_handler, (void *) H_IS_CAS);
  add_write_handler("ignore_add", write_handler, (void *) H_IGNORE_ADD);
  add_write_handler("ignore_del", write_handler, (void *) H_IGNORE_DEL);
  add_write_handler("ignore_clear", write_handler, (void *) H_IGNORE_CLEAR);
  add_write_handler("allow_add", write_handler, (void *) H_ALLOW_ADD);
  add_write_handler("allow_del", write_handler, (void *) H_ALLOW_DEL);
  add_write_handler("allow_clear", write_handler, (void *) H_ALLOW_CLEAR);
}
*/
CLICK_ENDDECLS
ELEMENT_REQUIRES(SR2LinkTableMulti)
EXPORT_ELEMENT(SR2ChannelAssignerMulti)
