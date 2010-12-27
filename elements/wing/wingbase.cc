/*
 * wingbase.{cc,hh} 
 * Roberto Riggio
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
 * Copyright (c) 2009 CREATE-NET
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
#include "wingbase.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
CLICK_DECLS

WINGBase::WINGBase() :
	_link_table(0), _arp_table(0), _debug(false) {
}

WINGBase::~WINGBase() {
}

void
WINGBase::set_ethernet_header(WritablePacket * p, NodeAddress src, NodeAddress dst)
{

	click_ether *eh = (click_ether *) p->data();

	EtherAddress eth_src = _arp_table->lookup(src);
	if (src && eth_src.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for src %s (%s)", 
				this,
				__func__,
				src.unparse().c_str(),
				eth_src.unparse().c_str());
	}

	EtherAddress eth_dst = _arp_table->lookup(dst);
	if (dst && eth_dst.is_group()) {
		click_chatter("%{element} :: %s :: arp lookup failed for dst %s (%s)", 
				this,
				__func__,
				dst.unparse().c_str(),
				eth_dst.unparse().c_str());

	}

	memcpy(eh->ether_dhost, eth_dst.data(), 6);
	memcpy(eh->ether_shost, eth_src.data(), 6);

}

Packet *
WINGBase::encap(Packet *p_in, PathMulti best)
{
	if (best[0].dep()._ip != _ip) {
		click_chatter("%{element} :: %s :: first hop %s doesn't match my ip %s", 
				this,
				__func__,
				best[0].dep().unparse().c_str(), 
				_ip.unparse().c_str());
		p_in->kill();
		return 0;
	}

	int hops = best.size() - 1;
	int len = wing_data::len_wo_data(hops);
	int data_len = p_in->length();

	WritablePacket *p = p_in->push(len + sizeof(click_ether));
	if (p == 0) {
		click_chatter("%{element} :: %s :: cannot encap packet!", this, __func__);
		return 0;
	}

	click_ether *eh = (click_ether *) p->data();
	struct wing_data *pk = (struct wing_data *) (eh + 1);
	memset(pk, '\0', len);

	pk->_type = WING_PT_DATA;
	pk->set_data_len(data_len);
	pk->set_num_links(hops);

	for (int i = 0; i < hops; i++) {
		pk->set_link(i, best[i].dep(), best[i+1].arr(), _link_table->get_link_channel(best[i].dep(), best[i+1].arr()));
	}

	set_ethernet_header(p, best[0].dep(), best[1].arr());

	return p;

}

Packet * WINGBase::create_wing_packet(NodeAddress src, NodeAddress dst, int type, NodeAddress qdst, NodeAddress netmask, NodeAddress qsrc, int seq, PathMulti best) {

	if (!src) {
		click_chatter("%{element} :: %s :: bad source address",
			      this,
			      __func__);
		return 0;
	}

	if ((best.size() > 0) && !_link_table->valid_route(best)) {
		click_chatter("%{element} :: %s :: invalid route %s", 
				this,
				__func__, 
				route_to_string(best).c_str());
		return 0;
	}

	int hops = (best.size() > 0) ? best.size() - 1 : 0;
	int len = wing_packet::len_wo_data(hops);

	WritablePacket *p = Packet::make(len + sizeof(click_ether));

	if (p == 0) {
		click_chatter("%{element} :: %s :: cannot make packet!", this, __func__);
		return 0;
	}

	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	memset(pk, '\0', len);

	pk->_type = type;
	pk->set_qdst(qdst);
	pk->set_netmask(netmask);
	pk->set_qsrc(qsrc);
	pk->set_seq(seq);
	pk->set_num_links(hops);

	for (int i = 0; i < hops; i++) {
		pk->set_link(i, best[i].dep(), best[i + 1].arr(), 
				_link_table->get_link_metric(best[i].dep(), best[i + 1].arr()), 
				_link_table->get_link_metric(best[i + 1].arr(), best[i].dep()), 
				_link_table->get_link_seq(best[i].dep(), best[i + 1].arr()), 
				_link_table->get_link_age(best[i].dep(), best[i + 1].arr()),
				_link_table->get_link_channel(best[i].dep(), best[i + 1].arr()));
	}

	set_ethernet_header(p, src, dst);

	return p;

}

bool WINGBase::update_link_table(Packet *p) {
	click_ether *eh = (click_ether *) p->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	/* update the metrics from the packet */
	for (int i = 0; i < pk->num_links(); i++) {
		NodeAddress a = pk->get_link_dep(i);
		NodeAddress b = pk->get_link_arr(i);
		uint32_t fwd_m = pk->get_link_fwd(i);
		uint32_t rev_m = pk->get_link_rev(i);
		uint32_t seq = pk->get_link_seq(i);
		uint32_t age = pk->get_link_age(i);
		uint32_t channel = pk->get_link_channel(i);
		if (!fwd_m || !rev_m || !seq || !channel) {
			click_chatter("%{element} :: %s :: invalid link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					rev_m,
					seq,
					channel,
					b.unparse().c_str());
			return false;
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					seq,
					age,
					fwd_m,
					channel,
					b.unparse().c_str());
		}
		if (fwd_m && !_link_table->update_link(a, b, seq, age, fwd_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update fwd_m %s > %d > %s",
					this, 
					__func__, 
					a.unparse().c_str(), 
					fwd_m,
					b.unparse().c_str());
		}
		if (_debug) {
			click_chatter("%{element} :: %s :: updating link %s > (%u, %u, %u, %u) > %s",
					this, 
					__func__, 
					b.unparse().c_str(), 
					seq,
					age,
					rev_m,
					channel,
					a.unparse().c_str());
		}
		if (rev_m && !_link_table->update_link(b, a, seq, age, rev_m, channel)) {
			click_chatter("%{element} :: %s :: couldn't update rev_m %s < %d < %s",
					this, 
					__func__, 
					b.unparse().c_str(), 	
					rev_m,
					a.unparse().c_str());
		}
	}
	return true;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti NodeAddress)
ELEMENT_PROVIDES(WINGBase)
