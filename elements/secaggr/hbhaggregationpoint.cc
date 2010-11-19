/*
 * hbhaggregationpoint.{cc,hh}
 *
 * Roberto Riggio
 * Copyright (c) 2008, CREATE-NET
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions    
 * are met:
 * 
 *   - Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright 
 *     notice, this list of conditions and the following disclaimer in 
 *     the documentation and/or other materials provided with the 
 *     distribution.
 *   - Neither the name of the CREATE-NET nor the names of its 
 *     contributors may be used to endorse or promote products derived 
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <click/config.h>
#include "hbhaggregationpoint.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/vector.hh>
#include "sapacket.hh"
#include "availablesensorsindex.hh"
CLICK_DECLS

HBHAggregationPoint::HBHAggregationPoint() 
  :  _jitter(2500)
{
  _inc_table = new AppTable();
  _fwd_table = new AppTable();
  _queue_pool = new QueuePool();
  reset();
}

HBHAggregationPoint::~HBHAggregationPoint()
{
  _map_lock.acquire_write();

  // de-allocate inc-table
  AppItr itr = _inc_table->begin();
  while(itr != _inc_table->end()){
    release_queue(itr.value());
    itr++;
  } // end while
  _inc_table->clear();
  delete _inc_table;

  // de-allocate fwd-table
  itr = _fwd_table->begin();
  while(itr != _fwd_table->end()){
    release_queue(itr.value());
    itr++;
  } // end while
  _fwd_table->clear();
  delete _fwd_table;

  // de-allocate pool
  clean_pool();
  _queue_pool->clear();
  delete _queue_pool;
  _map_lock.release_write();

}

void 
HBHAggregationPoint::clean_pool(){
  _pool_lock.acquire_write();
  PoolItr itr = _queue_pool->begin();
  while(itr != _queue_pool->end()){
    Packet* p = 0;
    // destroy queued packets
    do{
      p = (*itr)->pull();
      // flush
      if(p){
	    p->kill();
      } // end if
    }while(p);
    itr++;
  } // end while
  _pool_lock.release_write();
}

void * 
HBHAggregationPoint::cast(const char *n)
{
    if (strcmp(n, "HBHAggregationPoint") == 0)
	return (HBHAggregationPoint *)this;
    else
	return NotifierQueue::cast(n);
}

void 
HBHAggregationPoint::reset()
{
  _drops=0;
  _creates=0;
  _deletes=0;
  _sleepiness=0;

  if (!_inc_table->empty()) 
    _next = _inc_table->begin().key();
  else if (!_fwd_table->empty()) 
    _next = _fwd_table->begin().key();
  else
    _next = 0;
}

int 
HBHAggregationPoint::configure(Vector<String>& conf, ErrorHandler* errh) 
{
	_debug = false;
	_capacity = 1000;
	_empty_note.initialize(Notifier::EMPTY_NOTIFIER, router());

	int ret = cp_va_kparse(conf, this, errh,
				"CAPACITY", 0, cpUnsigned, &_capacity,
				"JITTER", 0, cpUnsigned, &_jitter,
				"INDEX", 0, cpElement, &_as_table,
				"DEBUG", 0, cpBool, &_debug,
				cpEnd);

	if (!_as_table) 
		return errh->error("INDEX not specified");
	if (_as_table->cast("AvailableSensorsIndex") == 0) 
		return errh->error("INDEX element is not a AvailableSensorsIndex");
	return ret;
}

void 
HBHAggregationPoint::push(int port, Packet* p)
{ 

  struct sapacket *sa = (sapacket *)p->data();

  if ((sa->type() != SA_PT_PACK_N) && (sa->type() != SA_PT_PACK_P)) {
    click_chatter("%{element} :: %s :: unknown packet type", this, __func__);
    p->kill();
    return;
  }

  _map_lock.acquire_write();

  AppTable *table = (port == 0) ? _inc_table : _fwd_table;
  HBHAggregationPointQueue* q = table->get(sa->msg());
  
  // no queue, get one
  if(!q){
    HBHAggregationPointQueue* q_inc = request_queue();
    HBHAggregationPointQueue* q_fwd = request_queue();
    if(_inc_table->empty() && _fwd_table->empty())
        _next = sa->msg();
    _inc_table->find_insert(sa->msg(), q_inc);  
    _fwd_table->find_insert(sa->msg(), q_fwd);  
    q = (port == 0) ? q_inc : q_fwd;
  } // end if 

  if (!p->timestamp_anno()) {
    p->timestamp_anno().assign_now();
  }  

  // push packet on queue or fail
  if(q->push(p)){
    _empty_note.wake(); 
    _sleepiness = 0;
  }else{
    // queue overflow, destroy the packet
    if (_drops == 0)
      click_chatter("%{element}: overflow", this);
    p->kill();
    _drops++;
  } // end if
  _map_lock.release_write();

}

Packet * 
HBHAggregationPoint::pull(int)
{
    bool trash = false;
    if (_next == 0) {
      if (++_sleepiness == SLEEPINESS_TRIGGER) {
        _empty_note.sleep();
      }
      return 0;
    }
    _map_lock.acquire_write();
    AppItr active_inc = _inc_table->find(_next);
    AppItr active_fwd = _fwd_table->find(_next);
    int key_inc = active_inc.key();
    int key_fwd = active_fwd.key();
    HBHAggregationPointQueue* queue_inc = active_inc.value();
    HBHAggregationPointQueue* queue_fwd = active_fwd.value();
    if (queue_inc->burst_ready(_jitter) || queue_fwd->burst_ready(_jitter)) {
        Packet *inc = queue_inc->pull();
        Packet *fwd = queue_fwd->pull();
        uint32_t avg = 0;
        uint32_t var = 0;
        Vector<uint8_t> *sensors_inc = new Vector<uint8_t>();
        if (inc) {
          struct sapacket *pk = (struct sapacket *) inc->data();
          avg = sa_add_ciphers(avg, pk->avg());
          var = sa_add_ciphers(var, pk->var());
          for (int i = 0; i < pk->sensors(); i++) {
            sensors_inc->push_front(pk->get_sensor(i));
          }
        }
        Vector<uint8_t> *sensors_fwd = new Vector<uint8_t>();
        if (fwd) {
          struct sapacket *pk = (struct sapacket *) fwd->data();
          avg = sa_add_ciphers(avg, pk->avg());
          var = sa_add_ciphers(var, pk->var());
          for (int i = 0; i < pk->sensors(); i++) {
            sensors_fwd->push_front(pk->get_sensor(i));
          }
        }
        Vector<uint8_t> *diff = new Vector<uint8_t>();
	uint32_t size = 0;
	AvailableSensors* as = _as_table->get(key_inc);
        if (fwd) {
          diff = as->diff(sensors_inc, false);
	  size = diff->size() + sensors_fwd->size();
	} else {
          diff = as->diff(sensors_inc, true);
	  size = diff->size();
	}
        WritablePacket *q = Packet::make(sapacket::len(size));
        struct sapacket *wp = (struct sapacket *) q->data();
        memset(wp, '\0', sapacket::len(1));
        wp->set_version(_version);
        wp->set_type(SA_PT_PACK_N);
        wp->set_sensors(size);
        for (int i = 0; i < diff->size(); i++) {
	  wp->set_sensor(i, diff->at(i));
        }
        if (fwd) {
          for (int i = 0; i < sensors_fwd->size(); i++) {
  	    wp->set_sensor(diff->size()+i, sensors_fwd->at(i));
          }
	}
        wp->set_msg(key_inc);
        wp->set_avg(avg);
        wp->set_var(var);
        _map_lock.release_write();
        return q;
    } 
    if ((queue_inc->_size == 0) && 
	(queue_fwd->_size == 0) && 
	(++queue_inc->_trash == TRASH_TRIGGER) && 
	(++queue_fwd->_trash == TRASH_TRIGGER)) {
      trash = true;
    }
    active_inc++;
    active_fwd++;
    if (trash) {
        release_queue(queue_inc);
        release_queue(queue_fwd);
        _inc_table->erase(key_inc);
        _fwd_table->erase(key_fwd);
    } 
    if (_inc_table->empty() && _fwd_table->empty()) {
        _next = 0;
        _map_lock.release_write();
        return 0;
    }
    if (active_inc == _inc_table->end()) {
        active_inc = _inc_table->begin();
    }
    _next = active_inc.key();
    _map_lock.release_write();
    return 0;
}

HBHAggregationPointQueue * 
HBHAggregationPoint::request_queue() 
{
  HBHAggregationPointQueue* q = 0;
  _pool_lock.acquire_write();
  if(_queue_pool->size() == 0){
    _queue_pool->push_back(new HBHAggregationPointQueue(_capacity));
    _creates++;
  } // end if
  // check out queue and remove from queue pool
  q = _queue_pool->back();
  _queue_pool->pop_back();
  _pool_lock.release_write();
  q->reset();
  return(q);
}

void 
HBHAggregationPoint::release_queue(HBHAggregationPointQueue* q)
{
  _pool_lock.acquire_write();    
  _queue_pool->push_back(q);
  _pool_lock.release_write();
}

enum { H_RESET, H_DROPS, H_CREATES, H_DELETES, H_CAPACITY, H_MAX_BURST_SIZE, H_MIN_BURST_SIZE, H_MAX_DELAY };

void 
HBHAggregationPoint::add_handlers()
{
  add_read_handler("reset", read_handler, (void*)H_RESET);
  add_read_handler("queue_creates", read_handler, (void*)H_CREATES);
  add_read_handler("queue_deletes", read_handler, (void*)H_DELETES);
  add_read_handler("drops", read_handler, (void*)H_DROPS);
  add_read_handler("capacity", read_handler, (void*)H_CAPACITY);
}

String 
HBHAggregationPoint::read_handler(Element *e, void *thunk)
{
  HBHAggregationPoint *c = (HBHAggregationPoint *)e;
  switch ((intptr_t)thunk) {
  case H_RESET:
    c->reset();
    return(String(""));
  case H_CREATES:
    return(String(c->creates()) + "\n");
  case H_DELETES:
    return(String(c->deletes()) + "\n");
  case H_DROPS:
    return(String(c->drops()) + "\n");
  case H_CAPACITY:
    return(String(c->capacity()) + "\n");
  default:
    return "<error>\n";
  }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue)
EXPORT_ELEMENT(HBHAggregationPoint)
