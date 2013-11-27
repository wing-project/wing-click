/*
 * empowerlvapmanager.{cc,hh} -- manages LVAPS (EmPOWER Access Point)
 * Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET

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
#include "empowerlvapmanager.hh"
#include <click/straccum.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <clicknet/ether.h>
#include <click/etheraddress64.hh>
#include "empowerpacket.hh"
#include "empowerbeaconsource.hh"
#include "empoweropenauthresponder.hh"
#include "empowerassociationresponder.hh"
#include "empowerpowersavebuffer.hh"
CLICK_DECLS

EmpowerLVAPManager::EmpowerLVAPManager() :
	_ebs(0), _eauthr(0), _eassor(0), _epsb(0), _mask(EtherAddress::make_broadcast()),
	_timer(this), _debugfs_string(""), _seq(0), _port(0), _period(5000),
	_debug(false) {
}

EmpowerLVAPManager::~EmpowerLVAPManager() {
}

int EmpowerLVAPManager::initialize(ErrorHandler *) {
	_timer.initialize(this);
	_timer.schedule_now();
	compute_bssid_mask();
	return 0;
}

void EmpowerLVAPManager::run_timer(Timer *) {
	// send hello packet
	send_hello();
	// re-schedule the timer with some jitter
	unsigned max_jitter = _period / 10;
	unsigned j = click_random(0, 2 * max_jitter);
	_timer.schedule_after_msec(_period + j - max_jitter);
}

int EmpowerLVAPManager::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh).read_m("HWADDR", _hwaddr)
			                     .read_m("EBS", ElementCastArg("EmpowerBeaconSource"), _ebs)
			                     .read_m("EAUTHR", ElementCastArg("EmpowerOpenAuthResponder"), _eauthr)
			                     .read_m("EASSOR", ElementCastArg("EmpowerAssociationResponder"), _eassor)
			                     .read_m("DEBUGFS", _debugfs_string)
			                     .read("EPSB", ElementCastArg("EmpowerPowerSaveBuffer"), _epsb)
								 .read("PERIOD", _period)
			                     .read("DEBUG", _debug)
			                     .complete();

}

void EmpowerLVAPManager::send_association_request(EtherAddress src, String ssid) {

	WritablePacket *p = Packet::make(sizeof(empower_assoc_request) + ssid.length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_assoc_request *request = (struct empower_assoc_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_assoc_request) + ssid.length());
	request->set_type(EMPOWER_PT_ASSOC_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_hwaddr);
	request->set_sta(src);
	request->set_ssid(ssid);

	output(0).push(p);

}

void EmpowerLVAPManager::send_auth_request(EtherAddress src) {

	WritablePacket *p = Packet::make(sizeof(empower_auth_request));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_auth_request *request = (struct empower_auth_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_auth_request));
	request->set_type(EMPOWER_PT_AUTH_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_hwaddr);
	request->set_sta(src);

	output(0).push(p);

}

void EmpowerLVAPManager::send_probe_request(EtherAddress src, String ssid) {

	WritablePacket *p = Packet::make(sizeof(empower_probe_request) + ssid.length());

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_probe_request *request = (struct empower_probe_request *) (p->data());
	request->set_version(_empower_version);
	request->set_length(sizeof(empower_probe_request) + ssid.length());
	request->set_type(EMPOWER_PT_PROBE_REQUEST);
	request->set_seq(get_next_seq());
	request->set_wtp(_hwaddr);
	request->set_sta(src);
	request->set_ssid(ssid);

	output(0).push(p);

}

void EmpowerLVAPManager::send_hello() {

	if (_port == 0 || _dpid == EtherAddress64())
		return;

	WritablePacket *p = Packet::make(sizeof(empower_hello));

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
				      this,
				      __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_hello *hello = (struct empower_hello *) (p->data());

	hello->set_version(_empower_version);
	hello->set_length(sizeof(empower_hello));
	hello->set_type(EMPOWER_PT_HELLO);
	hello->set_seq(get_next_seq());
	hello->set_port(_port);
	hello->set_wtp(_hwaddr);
	hello->set_dpid(_dpid);

	checked_output_push(0, p);

}

void EmpowerLVAPManager::send_status_lvap(EtherAddress sta) {

	int len = sizeof(empower_status_lvap);

	EmpowerStationState ess = _lvaps.get(sta);

	len += 1 + ess._ssid.length();

	for (int i = 0; i < ess._ssids.size(); i++) {
		len += 1 + ess._ssids[i].length();
	}

	WritablePacket *p = Packet::make(len);

	if (!p) {
		click_chatter("%{element} :: %s :: cannot make packet!",
					  this,
					  __func__);
		return;
	}

	memset(p->data(), 0, p->length());

	empower_status_lvap *status = (struct empower_status_lvap *) (p->data());
	status->set_version(_empower_version);
	status->set_length(len);
	status->set_type(EMPOWER_PT_STATUS_LVAP);
	status->set_seq(get_next_seq());

	status->set_assoid(ess._assoc_id);

	if (ess._authentication_status)
		status->set_flag(EMPOWER_STATUS_LVAP_AUTHENTICATED);

	if (ess._association_status)
		status->set_flag(EMPOWER_STATUS_LVAP_ASSOCIATED);

	status->set_wtp(_hwaddr);
	status->set_sta(sta);
	status->set_bssid(ess._bssid);

	uint8_t *ptr = (uint8_t *) p->data();
	ptr += sizeof(struct empower_status_lvap);

	ptr[0] = ess._ssid.length();
	memcpy(ptr + 1, ess._ssid.data(), ess._ssid.length());
	ptr += 1 + ess._ssid.length();

	for (int i = 0; i < ess._ssids.size(); i++) {
		ptr[0] = ess._ssids[i].length();
		memcpy(ptr + 1, ess._ssids[i].data(), ess._ssids[i].length());
		ptr += 1 + ess._ssids[i].length();
	}

	output(0).push(p);

}

int EmpowerLVAPManager::handle_add_lvap(Packet *p, uint32_t offset) {

	struct empower_add_lvap *q = (struct empower_add_lvap *) (p->data() + offset);

	EtherAddress src = q->sta();
	EtherAddress bssid = q->bssid();
	Vector<String> ssids;

	uint8_t *ptr = (uint8_t *) q;
	uint8_t *end = ptr + q->length();

	ptr += sizeof(struct empower_add_lvap);

	String ssid = String((char *) ptr + 1, WIFI_MIN((int)*ptr, WIFI_NWID_MAXSIZE));
	ptr += (int)*ptr + 1;

	while (ptr != end) {
		if (ptr > end) {
			click_chatter("%{element} :: %s :: Invalid packet (%u)",
						  this,
						  __func__,
						  EMPOWER_PT_ADD_LVAP);
			return -1;
		}
		String tmp = String((char *) ptr + 1, WIFI_MIN((int)*ptr, WIFI_NWID_MAXSIZE));
		ssids.push_back(tmp);
		ptr += (int)*ptr + 1;
	}

	int asso_id = q->assoid();
	bool authentication_state = q->flag(EMPOWER_STATUS_LVAP_AUTHENTICATED);
	bool association_state = q->flag(EMPOWER_STATUS_LVAP_ASSOCIATED);

	// add new lvap
	add_lvap(src, bssid, ssid, ssids, asso_id, authentication_state, association_state);

	return 0;

}

int EmpowerLVAPManager::handle_del_lvap(Packet *p, uint32_t offset) {
	struct empower_del_lvap *q = (struct empower_del_lvap *) (p->data() + offset);
	del_lvap(q->sta());
	return 0;
}

int EmpowerLVAPManager::handle_probe_response(Packet *p, uint32_t offset) {
	struct empower_probe_response *q = (struct empower_probe_response *) (p->data() + offset);
	_ebs->send_beacon(q->sta(), true);
	return 0;
}

int EmpowerLVAPManager::handle_auth_response(Packet *p, uint32_t offset) {
	struct empower_auth_response *q = (struct empower_auth_response *) (p->data() + offset);
	_eauthr->send_auth_response(q->sta(), 2, WIFI_STATUS_SUCCESS);
	return 0;
}

int EmpowerLVAPManager::handle_assoc_response(Packet *p, uint32_t offset) {
	struct empower_assoc_response *q = (struct empower_assoc_response *) (p->data() + offset);
	_eassor->send_association_response(q->sta(), WIFI_STATUS_SUCCESS);
	return 0;
}

void EmpowerLVAPManager::push(int, Packet *p) {

	/* This is a control packet coming from a Socket
	 * element.
	 */

	if (p->length() < sizeof(struct empower_header)) {
		click_chatter("%{element} :: %s :: Packet too small: %d Vs. %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct empower_header));
		p->kill();
		return;
	}

	uint32_t offset = 0;

	while (offset < p->length()) {
		struct empower_header *w = (struct empower_header *) (p->data() + offset);
		switch (w->type()) {
		case EMPOWER_PT_ADD_LVAP:
			handle_add_lvap(p, offset);
			break;
		case EMPOWER_PT_DEL_LVAP:
			handle_del_lvap(p, offset);
			break;
		case EMPOWER_PT_PROBE_RESPONSE:
			handle_probe_response(p, offset);
			break;
		case EMPOWER_PT_AUTH_RESPONSE:
			handle_auth_response(p, offset);
			break;
		case EMPOWER_PT_ASSOC_RESPONSE:
			handle_assoc_response(p, offset);
			break;
		default:
			click_chatter("%{element} :: %s :: Unknown packet type: %d",
					      this,
					      __func__,
					      w->type());
		}
		offset += w->length();
	}

	p->kill();
	return;

}

/*
 * This re-computes the BSSID mask for this node
 * using all the BSSIDs of the VAPs, and sets the
 * hardware register accordingly.
 */
void EmpowerLVAPManager::compute_bssid_mask() {

	uint8_t bssid_mask[6];
	int i;

	// Start with a mask of ff:ff:ff:ff:ff:ff
	for (i = 0; i < 6; i++) {
		bssid_mask[i] = 0xff;
	}

	// For each VAP, update the bssid mask to include
	// the common bits of all VAPs.
	for (LVAPSIter it = _lvaps.begin(); it.live(); it++) {
		for (i = 0; i < 6; i++) {
			const uint8_t *hw = (const uint8_t *) _hwaddr.data();
			const uint8_t *bssid =
					(const uint8_t *) it.value()._bssid.data();
			bssid_mask[i] &= ~(hw[i] ^ bssid[i]);
		}
	}

	_mask = EtherAddress(bssid_mask);

	// Update bssid mask register through debugfs
	FILE *debugfs_file = fopen(_debugfs_string.c_str(), "w");

	if (debugfs_file != NULL) {
		if (_debug) {
			click_chatter("%{element} :: %s :: %s",
						  this,
						  __func__,
						  _mask.unparse_colon().c_str());
		}
		fprintf(debugfs_file, "%s\n", _mask.unparse_colon().c_str());
		fclose(debugfs_file);
		return;
	}

	click_chatter("%{element} :: %s :: unable to open debugfs file %s",
				  this,
				  __func__,
				  _debugfs_string.c_str());

}

/** 
 * Invoking this implies adding a client
 * to the VAP.
 *
 * return -1 if the STA is already assigned
 */
int EmpowerLVAPManager::add_lvap(EtherAddress sta, EtherAddress bssid,
		String ssid, Vector<String> ssids, int assoc_id,
		bool authentication_state, bool association_state) {

	if (_debug) {
	    StringAccum sa;
    	sa << ssids[0];
	    for (int i = 1; i < ssids.size(); i++) {
	    	sa << ", " << ssids[i];
	    }
		click_chatter("%{element} :: %s :: sta %s bssid %s ssid %s [ %s ] assoc_id %d %s %s",
				      this,
				      __func__,
				      sta.unparse_colon().c_str(),
				      bssid.unparse().c_str(),
				      ssid.c_str(),
				      sa.take_string().c_str(),
				      assoc_id,
				      authentication_state ? "AUTH" : "NO_AUTH",
				      association_state ? "ASSOC" : "NO_ASSOC");
	}

	if (_lvaps.find(sta) == _lvaps.end()) {

		EmpowerStationState state;
		state._bssid = bssid;
		state._ssids = ssids;
		state._assoc_id = assoc_id;
		state._authentication_status = authentication_state;
		state._association_status = association_state;
		state._power_save = false;
		state._ssid = ssid;
		_lvaps.set(sta, state);

		/* Regenerate the BSSID mask */
		compute_bssid_mask();

		if (_epsb) {
			_epsb->request_queue(sta);
		}

		return 0;

	}

	EmpowerStationState *ess = _lvaps.get_pointer(sta);
	ess->_ssids = ssids;
	ess->_assoc_id = assoc_id;
	ess->_authentication_status = authentication_state;
	ess->_association_status = association_state;
	ess->_ssid = ssid;

	return 0;

}

/** 
 * Invoking this implies knocking
 * a client off the access point
 */
int EmpowerLVAPManager::del_lvap(EtherAddress sta) {

	if (_debug) {
		click_chatter("%{element} :: %s :: sta %s",
				      this,
				      __func__,
				      sta.unparse_colon().c_str());
	}

	// First make sure that this VAP isn't here already, in which
	// case we'll just ignore the request
	if (_lvaps.find(sta) == _lvaps.end()) {
		click_chatter("%{element} :: %s :: Ignoring LVAP delete request because the agent isn't hosting the LVAP",
				      this,
				      __func__);

		return -1;
	}

	_lvaps.erase(_lvaps.find(sta));

	// Remove this VAP's BSSID from the mask
	compute_bssid_mask();

	if (_epsb) {
		_epsb->release_queue(sta);
	}

	return 0;

}

enum {
	H_PORT,
	H_DPID,
	H_DEBUG,
	H_MASK,
	H_LVAPS,
	H_ADD_LVAP,
	H_DEL_LVAP
};

String EmpowerLVAPManager::read_handler(Element *e, void *thunk) {
	EmpowerLVAPManager *td = (EmpowerLVAPManager *) e;
	switch ((uintptr_t) thunk) {
	case H_PORT:
		return String(td->_port) + "\n";
	case H_DPID:
		return td->_dpid.unparse() + "\n";
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_MASK:
		return String(td->_mask.unparse()) + "\n";
	case H_LVAPS: {
	    StringAccum sa;
		for (LVAPSIter it = td->lvaps()->begin(); it.live(); it++) {
		    sa << "sta ";
		    sa << it.key().unparse();
		    sa << " bssid ";
		    sa << it.value()._bssid.unparse();
		    sa << " ssid ";
		    sa << it.value()._ssid;
		    sa << " ssids [ ";
	    	sa << it.value()._ssids[0];
		    for (int i = 1; i < it.value()._ssids.size(); i++) {
		    	sa << ", " << it.value()._ssids[i];
		    }
		    sa << " ] assoc_id ";
	    	sa << it.value()._assoc_id;
		    if (it.value()._power_save) {
		    	sa << " PS";
		    }
		    sa << "\n";
		}
		return sa.take_string();
	}
	default:
		return String();
	}
}

int EmpowerLVAPManager::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerLVAPManager *f = (EmpowerLVAPManager *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	case H_PORT: {
		int port;
		if (!IntArg().parse(s, port))
			return errh->error("port parameter must be unsigned");
		f->_port = port;
		break;
	}
	case H_DPID: {
		EtherAddress64 dpid;
		if (!EtherAddress64Arg().parse(s, dpid))
			return errh->error("error parsing address");
		f->_dpid = dpid;
		break;
	}
	}
	return 0;
}

void EmpowerLVAPManager::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_read_handler("port", read_handler, (void *) H_PORT);
	add_read_handler("dpid", read_handler, (void *) H_DPID);
	add_read_handler("lvaps", read_handler, (void *) H_LVAPS);
	add_read_handler("mask", read_handler, (void *) H_MASK);
	add_write_handler("port", write_handler, (void *) H_PORT);
	add_write_handler("dpid", write_handler, (void *) H_DPID);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerLVAPManager)
ELEMENT_REQUIRES(userlevel)
