/*
 * SR2QueryResponder.{cc,hh} -- DSR implementation
 * John Bicket
 *
 * Copyright (c) 1999-2001 Massachussr2queryresponders Institute of Technology
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
#include "sr2classifiermulti.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "availableinterfaces.hh"
#include "sr2packetmulti.hh"


CLICK_DECLS

SR2ClassifierMulti::SR2ClassifierMulti()
  :  _link_table(0)
{
}

SR2ClassifierMulti::~SR2ClassifierMulti()
{
}

int
SR2ClassifierMulti::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  _debug = false;
  ret = cp_va_kparse(conf, this, errh,
		     "IT", 0, cpElement, &_if_table,
		     "ARP", 0, cpElement, &_arp_table,
		     "DEBUG", 0, cpBool, &_debug,
		     "ISDEST", 0, cpBool, &_isdest,
		     cpEnd);

   if (_if_table && _if_table->cast("AvailableInterfaces") == 0) 
     return errh->error("AvailableInterfaces element is not an AvailableInterfaces");
   if (_arp_table && _arp_table->cast("ARPTableMulti") == 0) 
     return errh->error("ARPTable element is not an ARPtableMulti");

  //_ifaces = _if_table->get_if_list();

  return ret;
}

int
SR2ClassifierMulti::initialize (ErrorHandler *)
{
  return 0;
}

Packet *
SR2ClassifierMulti::rewrite_src(Packet *p){
    
    click_ether *eh = (click_ether *) p->data();
    
    EtherAddress eth = _if_table->lookup_def();
    
	  memcpy(eh->ether_shost, eth.data(), 6);
    
    return p;
}

Packet *
SR2ClassifierMulti::rewrite_dst(Packet *p){
    
    click_ether *eh = (click_ether *) p->data();
    
    EtherAddress eth = _arp_table->lookup_def_eth(EtherAddress(eh->ether_dhost));
    
	  memcpy(eh->ether_dhost, eth.data(), 6);
    
    return p;
}

int
SR2ClassifierMulti::get_output_incoming(EtherAddress eth){
  
  int output = 0;
  
  AvailableInterfaces::LocalIfInfo *ifinfo = _ifaces.findp(eth);
  
  output = (ifinfo->_iface / 256) - 1;
  
  return output;
  
}


Packet*
SR2ClassifierMulti::checkdst(Packet *p){
  
  click_ether *eh = (click_ether *) p->data();
  
  bool available = _if_table->check_remote_available(EtherAddress(eh->ether_dhost));
  
  if (!available){
    p = rewrite_dst(p);
  }
  
  return p;
  
}

int
SR2ClassifierMulti::checksrc(EtherAddress eth){
  
  int output = 0;
  
  AvailableInterfaces::LocalIfInfo *ifinfo = _ifaces.findp(eth);
  
  if (ifinfo->_available){
    output = (ifinfo->_iface / 256) - 1;
  }
  
  return output;
  
}

void 
SR2ClassifierMulti::push(int, Packet *p)
{
    
  typedef HashMap<EtherAddress,AvailableInterfaces::LocalIfInfo>::const_iterator IFTIter;
  bool found = false;

  _ifaces = _if_table->get_if_list();

  click_ether *eh = (click_ether *) p->data();
  //struct sr2packetmulti *pk = (struct sr2packetmulti *) (eh+1);

  unsigned char bcast_addr[6];
  bcast_addr[0] = 0xff;
  bcast_addr[1] = 0xff;
  bcast_addr[2] = 0xff;
  bcast_addr[3] = 0xff;
  bcast_addr[4] = 0xff;
  bcast_addr[5] = 0xff;
  
  // Classifier for broadcast incoming packets
  
  if (_isdest && !memcmp(eh->ether_dhost,&bcast_addr,6)) {
    found = true;
  	for (IFTIter iter = _ifaces.begin(); iter.live(); iter++){
  	  int if_output = (iter.value()._iface / 256) - 1;
  		output(if_output).push(p);
			return;
  	}
  }
  
  if(_isdest){
    
    // Classifier for incoming packets
    
    output(get_output_incoming(EtherAddress(eh->ether_dhost))).push(p);
		return;
    
  } else {
    
    // Classifier for outgoing packets
    
    //click_chatter("Outgoing source is: %s \n",EtherAddress(eh->ether_shost).unparse().c_str());
    
    int output_iface = checksrc(EtherAddress(eh->ether_shost));
    
    if (output_iface == 0){
      p = rewrite_src(p);
    }
    
    p = checkdst(p);

    output(output_iface).push(p);
		return;
    
  }
  
/*
  
  for(IFTIter iter = _ifaces.begin(); iter.live(); iter++){
    int if_output = (iter.value()._iface / 256) - 1;
	  if (_isdest) {
	    found = true;
		  if (!memcmp(eh->ether_dhost,iter.key().data(),6)) {
			  output(if_output).push(p);
		  }
	  } else {
      //if (!memcmp(eh->ether_shost,iter.key().data(),6)) {
		  if (pk->get_link_if(pk->next()-1) == iter.value()._iface) {
		    found = true;
			  if(iter.value()._available){
		        // interface is available
			      output(if_output).push(p);
			  } else {
			      output(0).push(divert_to_default(p));
			  }
		  }
	  }	
  }
  
  if (!found){
      output(0).push(divert_to_default(p));
  }
  
*/

}


CLICK_ENDDECLS
EXPORT_ELEMENT(SR2ClassifierMulti)

