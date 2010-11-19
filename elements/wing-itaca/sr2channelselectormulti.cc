/*
 * SR2ChannelSelectorMulti.{cc,hh} -- DSR implementation
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
#include "sr2channelselectormulti.hh"
#include "sr2classifiermulti.hh"

CLICK_DECLS

SR2ChannelSelectorMulti::SR2ChannelSelectorMulti()
  :  _ip(),
     _et(0),
     _link_table(0),
     _if_table(0),
     _arp_table(0),
     _timer(this)
{
  // Pick a starting sequence number that we have not used before.
  _seq = Timestamp::now().usec();
}

SR2ChannelSelectorMulti::~SR2ChannelSelectorMulti()
{
}

int
SR2ChannelSelectorMulti::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _is_cas = false;
	_debug = false;
  ret = cp_va_kparse(conf, this, errh,
		     "ETHTYPE", 0, cpUnsignedShort, &_et,
		     "IP", 0, cpIPAddress, &_ip,
		     "LT", 0, cpElement, &_link_table,
		     "IT", 0, cpElement, &_if_table,
		     "ARP", 0, cpElement, &_arp_table,
		     "PERIOD", 0, cpUnsigned, &_period,
		     "JITTER", 0, cpUnsigned, &_jitter,
		     "EXPIRE", 0, cpUnsigned, &_expire,
		     "CAS", 0, cpBool, &_is_cas,
		     "DEBUG", 0, cpBool, &_debug,
		     cpEnd);

  if (!_et) 
    return errh->error("ETHTYPE not specified");
  if (!_ip) 
    return errh->error("IP not specified");
  if (!_link_table) 
    return errh->error("LT not specified");
  if (_link_table->cast("SR2LinkTableMulti") == 0) 
    return errh->error("LinkTable element is not a SR2LinkTableMulti");
  if (_if_table && _if_table->cast("AvailableInterfaces") == 0) 
    return errh->error("AvailableInterfaces element is not an AvailableInterfaces");
  if (_arp_table && _arp_table->cast("ARPTableMulti") == 0) 
    return errh->error("ARPTable element is not an ARPtableMulti");

  return ret;
}

int
SR2ChannelSelectorMulti::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  _timer.schedule_now ();

  return 0;
}

void
SR2ChannelSelectorMulti::run_timer (Timer *)
{
  cleanup();
  if (_is_cas) {
    start_ad();
  }
  unsigned max_jitter = _period / 10;
  unsigned j = click_random(0, 2 * max_jitter);
  Timestamp delay = Timestamp::make_msec(_period + j - max_jitter);
  _timer.schedule_at(Timestamp::now() + delay);
}

void
SR2ChannelSelectorMulti::start_ad()
{

  HashMap<EtherAddress,AvailableInterfaces::LocalIfInfo> my_iftable = _if_table->get_if_list();

		int len = sr2packetmulti::len_wo_data(1);
		WritablePacket *p = Packet::make(len + sizeof(click_ether));
		if(p == 0)
		  return;
		click_ether *eh = (click_ether *) p->data();
		struct sr2packetmulti *pk = (struct sr2packetmulti *) (eh+1);
		memset(pk, '\0', len);
		pk->_version = _sr2_version;
		pk->_type = SR2_PT_CHBEACON;
		pk->unset_flag(~0);
		pk->set_qdst(_ip);
		pk->set_seq(++_seq);
		pk->set_num_links(0);
		pk->set_link_node(0,_ip);
		
		EtherAddress my_eth = _if_table->lookup_def();
		int my_iface = _if_table->lookup_def_id();
		
		pk->set_link_if(0,my_iface);

		send(p,my_eth);
  
}

void
SR2ChannelSelectorMulti::send(WritablePacket *p, EtherAddress my_eth)
{
  click_ether *eh = (click_ether *) p->data();
  eh->ether_type = htons(_et);
  memcpy(eh->ether_shost, my_eth.data(), 6);
  memset(eh->ether_dhost, 0xff, 6);
  output(0).push(p);
}

bool
SR2ChannelSelectorMulti::update_link(NodeAddress from, NodeAddress to, uint32_t seq, uint32_t metric) {
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
SR2ChannelSelectorMulti::set_local_scinfo(SR2ScanInfo scinfo)
{
	
	SR2ScanInfo * sctinfo = _local_scinfo.findp(scinfo._node);
	
	if (sctinfo){
		_local_scinfo.remove(sctinfo->_node._iface);
	}
	
	_local_scinfo.insert(scinfo._node._iface,scinfo);

}

HashMap<int,SR2ScanInfo>
SR2ChannelSelectorMulti::get_local_scinfo()
{
	HashMap<int,SR2ScanInfo> scinfo_table;
	for (HashMap<int,SR2ScanInfo>::const_iterator iter = _local_scinfo.begin(); iter.live(); iter++){
		scinfo_table.insert(iter.key(),iter.value());
	}
	_local_scinfo.clear();
	return scinfo_table;
}

void
SR2ChannelSelectorMulti::forward_ad_hook() 
{
    Timestamp now = Timestamp::now();
    for (int x = 0; x < _seen.size(); x++) {
    	if (_seen[x]._to_send < now && !_seen[x]._forwarded) {
    	    forward_ad(&_seen[x]);
    	}
    }
}

void
SR2ChannelSelectorMulti::forward_ad(Seen *s)
{

  s->_forwarded = true;
  _link_table->dijkstra(false);
  IPAddress src = s->_cas;
  SR2PathMulti best = _link_table->best_route(src, false);
  if (!_link_table->valid_route(best)) {
    click_chatter("%{element} :: %s :: invalid route from src %s\n",
		  this,
		  __func__,
		  src.unparse().c_str());
    return;
  }
  int links = best.size() - 1;

  int len = sr2packetmulti::len_wo_data(links);
  WritablePacket *p = Packet::make(len + sizeof(click_ether));
  if(p == 0)
    return;
  click_ether *eh = (click_ether *) p->data();
  struct sr2packetmulti *pk = (struct sr2packetmulti *) (eh+1);
  memset(pk, '\0', len);
  pk->_version = _sr2_version;
  pk->_type = SR2_PT_CHBEACON;
  pk->unset_flag(~0);
  pk->set_qdst(s->_cas);
  pk->set_seq(s->_seq);
  pk->set_num_links(links);

  for(int i=0; i < links; i++) {
    pk->set_link(i, best[i].get_dep(), best[i+1].get_arr(),
		  _link_table->get_link_metric(best[i].get_dep(), best[i+1].get_arr()),
		  _link_table->get_link_metric(best[i+1].get_dep(), best[i].get_arr()),
		  _link_table->get_link_seq(best[i].get_dep(), best[i+1].get_arr()),
		  _link_table->get_link_age(best[i].get_dep(), best[i+1].get_arr()));
  }

  //EtherAddress my_eth = _if_table->lookup_if(best[links].get_arr()._iface);
	EtherAddress my_eth = _if_table->lookup_def();

  send(p,my_eth);
}

IPAddress
SR2ChannelSelectorMulti::best_cas() 
{
	
  IPAddress best_cas = IPAddress();
  int best_metric = 0;
  Timestamp now = Timestamp::now();
  
  for(CASIter iter = _castable.begin(); iter.live(); iter++) {
    CASInfo nfo = iter.value();;
    Timestamp expire = nfo._last_update + Timestamp::make_msec(_expire);
    SR2PathMulti p = _link_table->best_route(nfo._ip, false);;
    int metric = _link_table->get_route_metric(p);
    if (now < expire &&
				metric && 
				((!best_metric) || best_metric > metric) &&
				!_ignore.findp(nfo._ip) &&
				(!_allow.size() || _allow.findp(nfo._ip))) {
			     best_cas = nfo._ip;
			     best_metric = metric;
    }
  }
  
  return best_cas;
}

void 
SR2ChannelSelectorMulti::cleanup() {

  CASTable new_table;
  Timestamp now = Timestamp::now();
  for(CASIter iter = _castable.begin(); iter.live(); iter++) {
    CASInfo nfo = iter.value();
    Timestamp expire = nfo._last_update + Timestamp::make_msec(_expire);  
    if (now < expire) {
      new_table.insert(nfo._ip, nfo);
    }
  }
  _castable.clear();
  for(CASIter iter = new_table.begin(); iter.live(); iter++) {
    CASInfo nfo = iter.value();
    _castable.insert(nfo._ip, nfo);
  }
}

void
SR2ChannelSelectorMulti::send_change_warning(bool start, bool scan, SR2ChannelAssignment ch_ass){

    SR2ChannelChangeWarning ch_warn = SR2ChannelChangeWarning(ch_ass, start, scan);
    int len = sr2packetmulti::len_wo_data(1);
    int data_len = sizeof(ch_warn);
    WritablePacket *p = Packet::make(len + data_len + sizeof(click_ether));
    if(p == 0)
      return;
    click_ether *eh = (click_ether *) p->data();
    struct sr2packetmulti *pk = (struct sr2packetmulti *) (eh+1);
    memset(pk, '\0', len);
    pk->_version = _sr2_version;
    pk->_type = SR2_PT_CHNGWARN;
    pk->unset_flag(~0);
    pk->set_data_len(data_len);
    pk->set_qdst(_ip);
    pk->set_seq(0);
    pk->set_num_links(0);
    pk->set_link_node(0,_ip);
		pk->set_link_if(0,_if_table->lookup_def_id());

    memcpy(p->data()+len+sizeof(click_ether), &ch_warn, data_len);

    
    EtherAddress shost = _if_table->lookup_def();

		eh->ether_type = htons(_et);
    memset(eh->ether_dhost, 0xff, 6); 
    memcpy(eh->ether_shost, shost.data(), 6);

    output(0).push(p);

}

void
SR2ChannelSelectorMulti::handle_change_warning(bool start, bool scan, SR2ChannelAssignment ch_ass){
  
  ChangingChannel cchannel = ChangingChannel (ch_ass._node._ipaddr,ch_ass._node._iface,ch_ass._new_iface);
  
	EtherAddress eth;

	if (start || scan) {

		eth = _arp_table->lookup(ch_ass._node);
	} else {

		eth = _arp_table->lookup(NodeAddress(ch_ass._node._ipaddr,ch_ass._new_iface));
	}
	
	bool available = _if_table->check_remote_available(eth);
	
	if ((start && !available) || (!start && available)) {
		return;
	}
	
  if (eth.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for %s",
			      this, 
			      __func__,
			      eth.unparse().c_str());
            return;
	}
	
	if (_debug ){
		if (start && scan){
			click_chatter("%{element} :: Neighbor node %s-%d unavailable, it started scanning\n",
	      			this, 
							ch_ass._node._ipaddr.unparse().c_str(),
							ch_ass._node._iface);
		}
		if (!start && scan){
			click_chatter("%{element} :: Neighbor node %s-%d unavailable, it stopped scanning\n",
	      			this, 
							ch_ass._node._ipaddr.unparse().c_str(),
							ch_ass._node._iface);
		}
		if (start && !scan){
			click_chatter("%{element} :: Neighbor node %s-%d unavailable, it started channel switch to %d\n",
	      			this, 
							ch_ass._node._ipaddr.unparse().c_str(),
							ch_ass._node._iface,
							ch_ass._new_iface);
		}
		if (!start && !scan){
			click_chatter("%{element} :: Neighbor node %s-%d unavailable, it ended channel switch to %d\n",
	      			this, 
							ch_ass._node._ipaddr.unparse().c_str(),
							ch_ass._node._iface,
							ch_ass._new_iface);
		}
	}
	
	// If it's only a channel scan, only set temporarily available/unavailable
	
	if (scan) {
			_if_table->set_remote_unavailable(eth,start,cchannel);
			return;
	}

	//
	// Updating all local tables with new remote interface channel
	//
	
	if ((!(eth.is_group())) && !scan) {
		if (start) {

			_if_table->set_remote_unavailable(eth,start,cchannel);
			_arp_table->change_if(ch_ass._node, ch_ass._new_iface);
			_link_table->change_if(ch_ass._node, ch_ass._new_iface);

		} else {

			_if_table->set_remote_unavailable(eth,start,cchannel);

		}
	}
	


}

void
SR2ChannelSelectorMulti::switch_channel(bool scheduled, SR2ChannelAssignment ch_ass)
{
	
	bool present = _if_table->check_if_present(ch_ass._node._iface);
	
	if ((ch_ass._node._iface == ch_ass._new_iface) || !present) {
		return;
	} else {
  
	  if (!(_if_table->check_if_available(ch_ass._node._iface)) && (!scheduled)) {
	
			// Interface not available, scheduling switch to a more convenient time
	    _if_table->set_channel_change(ch_ass._node._iface,ch_ass._new_iface);

	  } else {
		
			if (_debug){
				click_chatter("%{element} :: Starting Channel Switch %d >> %d\n",
		      			this,
								ch_ass._node._iface,
								ch_ass._new_iface);
			}

			// First Change Warning packet: start of channel switch
			for (int i=0; i<3; i++){
	    	send_change_warning(true, false, ch_ass);
			}

			// Disabling interface
			_if_table->set_unavailable(ch_ass._node._iface);
		
			// Updating all local tables with new local interface channel
	    _if_table->change_if(ch_ass._node._iface, ch_ass._new_iface);
			_link_table->change_if(ch_ass._node, ch_ass._new_iface);
			//_arp_table->change_if(ch_ass._node, ch_ass._new_iface);
		
			// Physical switch to the new channel
	    StringAccum switchcmd;
	    int channel_to_switch = ch_ass._new_iface % 256;
	    String chstr = _if_table->get_if_name(ch_ass._new_iface);
	    switchcmd << "iwconfig " << chstr << " channel " << channel_to_switch;

	    int switch_error = system (switchcmd.c_str());

	    if (switch_error){
	        click_chatter("%{element}: Channel switching failed using: %s\n",
	  		      this,
	  		      switchcmd.c_str());
	    } else {

	  			click_chatter("%{element}: Device %s switched on channel %d\n",
	  					this,
	  					chstr.c_str(),
	  					channel_to_switch);

	  	}

			// Re-enabling interface
	    _if_table->set_available(ch_ass._new_iface);

			// Second Change Warning packet: end of channel switch
			for (int i=0; i<3; i++){
	    	send_change_warning(false, false, ch_ass);
			}


	  }
  
	}

}

void
SR2ChannelSelectorMulti::push(int, Packet *p_in)
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
  if (pk->_type != SR2_PT_CHBEACON && pk->_type != SR2_PT_CHSCINFO && pk->_type != SR2_PT_CHASSIGN && pk->_type != SR2_PT_CHNGWARN) {
    click_chatter("%{element} :: %s :: back packet type %d",
		  this,
		  __func__,
		  pk->_type);
    p_in->kill();
    return;
  }
  if (_if_table->check_if_local(EtherAddress(eh->ether_shost))) {
    /*
		click_chatter("%{element} :: %s :: packet from me",
		  this,
		  __func__);
    p_in->kill();
		*/
    return;
  }

	if ((pk->_type != SR2_PT_CHNGWARN) && (pk->_type != SR2_PT_CHBEACON)) {
		for(int i = 0; i < pk->num_links(); i++) {
		  IPAddress a = pk->get_link_node(i);
		  uint16_t aif = pk->get_link_if(i);
		  IPAddress b = pk->get_link_node_b(i);
		  uint16_t bif = pk->get_link_if_b(i);
		  uint32_t fwd_m = pk->get_link_fwd(i);
		  uint32_t rev_m = pk->get_link_rev(i);
		  uint32_t seq = pk->get_link_seq(i);
		  if ((pk->_type == SR2_PT_CHBEACON) && (a == _ip || b == _ip || !fwd_m || !rev_m || !seq)) {
		    p_in->kill();
		    return;
		  }

		  if (fwd_m && !update_link(NodeAddress(a,aif),NodeAddress(b,bif),seq,fwd_m)) {
		    click_chatter("%{element} :: %s :: couldn't update fwd_m %s > %d > %s\n",
				  this,
				  __func__,
				  a.unparse().c_str(),
				  fwd_m,
				  b.unparse().c_str());
		  }
		  if (rev_m && !update_link(NodeAddress(b,bif),NodeAddress(a,aif),seq,rev_m)) {
		    click_chatter("%{element} :: %s :: couldn't update rev_m %s > %d > %s\n",
				  this,
				  __func__,
				  b.unparse().c_str(),
				  rev_m,
				  a.unparse().c_str());
		  }
		}
	}

    
  if (pk->_type == SR2_PT_CHBEACON){
	
		if (pk->num_links() == 0) {
			IPAddress neighbor = pk->get_link_node(pk->num_links());
			uint16_t neighborif = pk->get_link_if(pk->num_links());
		  if (_arp_table) {
		    _arp_table->insert(NodeAddress(neighbor,neighborif), EtherAddress(eh->ether_shost));
		  }
		}


    IPAddress cas = pk->qdst();
    if (!cas) {
  	  p_in->kill();
  	  return;
    }

    int si = 0;
    uint32_t seq = pk->seq();
    for(si = 0; si < _seen.size(); si++){
      if(cas == _seen[si]._cas && seq == _seen[si]._seq){
        _seen[si]._count++;
        p_in->kill();
        return;
      }
    }

    if (si == _seen.size()) {
      if (_seen.size() == 100) {
        _seen.pop_front();
        si--;
      }
      _seen.push_back(Seen(cas, seq, 0, 0));
    }
    _seen[si]._count++;

    CASInfo *nfo = _castable.findp(cas);
    if (!nfo) {
  	  _castable.insert(cas, CASInfo());
  	  nfo = _castable.findp(cas);
  	  nfo->_first_update = Timestamp::now();
    }
  
    nfo->_ip = cas;
    nfo->_last_update = Timestamp::now();
    nfo->_seen++;

    if (_is_cas) {
      p_in->kill();
      return;
    }

    /* schedule timer */
    int delay = click_random(1, _jitter);
  
    _seen[si]._to_send = _seen[si]._when + Timestamp::make_msec(delay);
    _seen[si]._forwarded = false;
    Timer *t = new Timer(static_forward_ad_hook, (void *) this);
    t->initialize(this);
    t->schedule_after_msec(delay);

    p_in->kill();
    return;
  }

  if (pk->_type == SR2_PT_CHSCINFO){
    
    if (pk->next() == (pk->num_links()-1)){
			if (pk->get_link_node_b(pk->num_links()-1) == _ip) {
				/* I am the ultimate consumer of this packet */
	  		//SET_MISC_IP_ANNO(p, pk->get_link_node(0));
				
				if (_debug){
					click_chatter("%{element} :: Received Scan Info from %s\n",
			      			this, 
									pk->get_link_node(0).unparse().c_str());
				}
				
	  		output(1).push(p_in);
	  		return;
			} else {
				// Packet was sent in broadcast but not for me
				p_in->kill();
				return;
			}

  	} 
  	pk->set_next(pk->next()+1);

		//EtherAddress eth_dest = _arp_table->lookup(NodeAddress(pk->get_link_node_b(pk->next()),pk->get_link_if_b(pk->next())));
   	EtherAddress eth_dest = _arp_table->lookup_def(NodeAddress(pk->get_link_node_b(pk->next()),pk->get_link_if_b(pk->next())));
		EtherAddress my_eth = _if_table->lookup_def();

  	memcpy(eh->ether_dhost, eth_dest.data(), 6);
  	memcpy(eh->ether_shost, my_eth.data(), 6);
  	output(0).push(p_in);
  	return;
  	
  }
  
  if (pk->_type == SR2_PT_CHASSIGN){

    SR2ChannelAssignment ch_ass;
    int data_len = sizeof(SR2ChannelAssignment);
    int head_len = pk->hlen_wo_data();

    memcpy(&ch_ass, p_in->data()+head_len+sizeof(click_ether), data_len);

    if (pk->next() == (pk->num_links()-1)){
			
			if (ch_ass._node._ipaddr == _ip) {
				/* I am the ultimate consumer of this packet */
				if (_debug){
					click_chatter("%{element} :: Received Channel Assignment %s-%d >> %d\n",
			      			this, 
									ch_ass._node._ipaddr.unparse().c_str(),
									ch_ass._node._iface,
									ch_ass._new_iface);
				}

				switch_channel(false, ch_ass);
	      return;
			} else {
				// Packet was sent in broadcast but not for me
				p_in->kill();
				return;
			}
  		

  	}

   	pk->set_next(pk->next() + 1);

		//EtherAddress eth_dest = _arp_table->lookup_def(NodeAddress(pk->get_link_node_b(pk->next()),pk->get_link_if_b(pk->next())));
		EtherAddress eth_dest = _arp_table->lookup_def(NodeAddress(pk->get_link_node_b(pk->next()),pk->get_link_if_b(pk->next())));
		EtherAddress my_eth = _if_table->lookup_def();
		
		memcpy(eh->ether_dhost, eth_dest.data(), 6);
  	memcpy(eh->ether_shost, my_eth.data(), 6);
  	output(0).push(p_in);
  	return;

  }

  if (pk->_type == SR2_PT_CHNGWARN){
	
			IPAddress neighbor = pk->get_link_node(0);
			uint16_t neighborif = pk->get_link_if(0);
		  if (_arp_table) {
		    _arp_table->insert(NodeAddress(neighbor,neighborif), EtherAddress(eh->ether_shost));
		  }

      SR2ChannelChangeWarning ch_warn;
      int data_len = sizeof(SR2ChannelChangeWarning);
      int head_len = pk->len_wo_data(1);
      memcpy(&ch_warn, p_in->data()+head_len+sizeof(click_ether), data_len);
			
			SR2ChannelAssignment ch_ass = ch_warn._ch_ass;
			bool start = ch_warn._start;
			bool scan = ch_warn._scan;
			
      handle_change_warning(start, scan, ch_ass);
  		return;
  		
  	}

}

String
SR2ChannelSelectorMulti::print_cas_stats()
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
SR2ChannelSelectorMulti::read_handler(Element *e, void *thunk)
{
  SR2ChannelSelectorMulti *f = (SR2ChannelSelectorMulti *)e;
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
SR2ChannelSelectorMulti::write_handler(const String &in_s, Element *e, void *vparam,
		     ErrorHandler *errh)
{
  SR2ChannelSelectorMulti *f = (SR2ChannelSelectorMulti *)e;
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
SR2ChannelSelectorMulti::add_handlers()
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

CLICK_ENDDECLS
ELEMENT_REQUIRES(SR2LinkTableMulti)
EXPORT_ELEMENT(SR2ChannelSelectorMulti)
