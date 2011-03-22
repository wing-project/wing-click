/*
 * fairbuffer.{cc,hh}
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
#include "fairbuffer.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/ether.h>
#include <elements/wing/arptablemulti.hh>
#include <elements/wing/linktablemulti.hh>
CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

enum { H_RESET, 
	H_DROPS, 
	H_BYTEDROPS, 
	H_CREATES, 
	H_DELETES, 
	H_CAPACITY, 
	H_LIST_QUEUE, 
	H_MAX_BURST_SIZE, 
	H_MIN_BURST_SIZE, 
	H_MAX_DELAY, 
	H_SCHEDULER_ACTIVE, 
	H_AGGREGATOR_ACTIVE 
};

FairBuffer::FairBuffer() : 
  _task(this), _timer(&_task)
{
  _fair_table = new FairTable();
  _head_table = new HeadTable();
  _queue_pool = new QueuePool();
  reset();
}

FairBuffer::~FairBuffer()
{
  _map_lock.acquire_write();

  // de-allocate fair-table
  TableItr itr = _fair_table->begin();
  while(itr != _fair_table->end()){
    release_queue(itr.value());
    itr++;
  } // end while
  _fair_table->clear();
  delete _fair_table;

  // de-allocate head-table
  HeadItr itr_head = _head_table->begin();
  while(itr_head != _head_table->end()) {
    Packet* p = itr_head.value();
    if(p){
      p->kill();
    }
    itr_head++;
  } // end while
  _head_table->clear();
  delete _head_table;

  // de-allocate pool
  clean_pool();
  _queue_pool->clear();
  delete _queue_pool;
  _map_lock.release_write();
}

uint32_t 
FairBuffer::compute_deficit(Packet* p)
{
  if (!p)
    return 0;
  if (!_lt)
    return p->length();
  IPAddress to = _arp_table->reverse_lookup(_next);
  return _lt->get_host_metric_from_me(to);
}

void 
FairBuffer::clean_pool(){
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
FairBuffer::cast(const char *n)
{
    if (strcmp(n, "FairBuffer") == 0)
	return (FairBuffer *)this;
    else if (strcmp(n, Notifier::FULL_NOTIFIER) == 0)
	return static_cast<Notifier*>(&_full_note);
    else
	return NotifierQueue::cast(n);
}

void 
FairBuffer::reset()
{
  _drops=0;
  _bdrops=0;
  _creates=0;
  _deletes=0;
  _sleepiness=0;

  if(!_fair_table->empty()) 
    _next = _fair_table->begin().key();
  else
    _next = EtherAddress();
}

int 
FairBuffer::configure(Vector<String>& conf, ErrorHandler* errh) 
{
  _lt=0;
  _et=0x0642;
  _arp_table=0;
  _capacity=500;
  _quantum=1500;
  _max_burst=1500;
  _min_burst=60;
  _max_delay=5;
  _scheduler_active = true;
  _aggregator_active = true;

  int res = cp_va_kparse(conf, this, errh,
			"ETHTYPE", 0, cpUnsignedShort, &_et,
			"CAPACITY", 0, cpUnsigned, &_capacity,
			"QUANTUM", 0, cpUnsigned, &_quantum,
			"MAX_BURST", 0, cpInteger, &_max_burst,
			"MIN_BURST", 0, cpInteger, &_min_burst,
			"DELAY", 0, cpUnsigned, &_max_delay,
			"SCHEDULER", 0, cpBool, &_scheduler_active,
			"AGGREGATOR", 0, cpBool, &_aggregator_active,
 			"LT", 0, cpElementCast, "LinkTableMulti", &_lt, 
 			"ARP", 0, cpElementCast, "ARPTableMulti", &_arp_table, 
			cpEnd);

  _full_note.initialize(Notifier::FULL_NOTIFIER, router());
  _full_note.set_active(true, false);
  _empty_note.initialize(Notifier::EMPTY_NOTIFIER, router());

  return res;

}

int
FairBuffer::initialize(ErrorHandler *errh)
{
    ScheduleInfo::initialize_task(this, &_task, errh);
    _timer.initialize(this);
    return 0;
}

void 
FairBuffer::push(int, Packet* p)
{ 

  click_ether *e = (click_ether *)p->data();
  EtherAddress dhost = EtherAddress(e->ether_dhost);

  _map_lock.acquire_write();
  // get queue for dhost
  FairBufferQueue* q = _fair_table->get(dhost);
  
  // no queue, get one
  if(!q){
    q = request_queue();
    if(_fair_table->empty()) 
      _next = dhost;
    _fair_table->find_insert(dhost, q);  
    _head_table->find_insert(dhost, 0);  
  } // end if 

  if (!p->timestamp_anno()) {
    p->timestamp_anno().assign_now();
  }  
  p->timestamp_anno() += Timestamp::make_msec(_max_delay);

  // push packet on queue or fail
  if(q->push(p)){
    _empty_note.wake();
    if (q->_size == q->_capacity) {
      _full_note.sleep();
    }
  }else{
    // queue overflow, destroy the packet
    if (_drops == 0)
      click_chatter("%{element}: overflow", this);
    _bdrops += p->length();
    p->kill();
    _drops++;
  } // end if
  _map_lock.release_write();

}

bool
FairBuffer::run_task(Task *)
{
  _empty_note.wake();
  return true;
}

Packet * 
FairBuffer::pull(int)
{

    if (_fair_table->empty()) {
        if (++_sleepiness == SLEEPINESS_TRIGGER) {
          _empty_note.sleep();
        }
        return 0;
    }

    bool trash = false;
    bool send = false;
    _map_lock.acquire_write();

    TableItr active = _fair_table->find(_next);
    HeadItr head = _head_table->find(_next);

    EtherAddress key = active.key();
    FairBufferQueue* queue = active.value();

    _sleepiness = 0;

    Packet *p = 0;
    if (head.value()) {
      p = head.value();
      _head_table->find_insert(_next, 0);
    } else if (queue->top() && (!_aggregator_active || queue->ready())) {
      // packet ready for output
      p = queue->aggregate(_et, _aggregator_active);
    }

    if (!p) {
      queue->_deficit = 0;
      if ((queue->_size == 0) && (++queue->_trash == TRASH_TRIGGER)) {
          trash = true;
      }
    } else if (!_scheduler_active || (compute_deficit(p) <= queue->_deficit)) {
      // packet ready for output
      queue->_deficit -= compute_deficit(p);
      _full_note.wake();
      send = true;
    } else {
      _head_table->find_insert(_next, p);
      queue->_trash = 0;
    }
  
    active++;

    if (trash) {
        release_queue(queue);
        _fair_table->erase(key);
        _head_table->erase(key);
    } 

    if (!_fair_table->empty()) {
      if (active == _fair_table->end()) {
        active = _fair_table->begin();
      }
      _next = active.key();
    } else {
      _next = EtherAddress();
    }

    // Check if there is queue that will expire earlier
    TableItr itr = _fair_table->begin();
    Timestamp expire = Timestamp(0);

    while(itr != _fair_table->end()) {
      if (itr.value()->top()) {
        if ((expire == Timestamp(0)) || (expire > itr.value()->top()->timestamp_anno())) {
          _next = itr.key();
          itr.value()->_deficit += _quantum;
          expire = itr.value()->top()->timestamp_anno();
        }
      }
      itr++;
    }

    Timestamp expiry = expire - Timer::adjustment();
    if (expiry > Timestamp::now()) {
      _timer.schedule_at(expiry);
    } else {
      _task.fast_reschedule();
    }

    _empty_note.sleep();
    _map_lock.release_write();

    return (send) ? p : 0;
}

String 
FairBuffer::list_queues()
{
  StringAccum result;

  _map_lock.acquire_read();
  TableItr itr = _fair_table->begin();
  result << "Key,Capacity,Packets,Bytes\n";
  while(itr != _fair_table->end()){
    result << (itr.key()).unparse() << ","
	   << itr.value()->_capacity << "," 
	   << itr.value()->_size << "," 
	   << itr.value()->_bsize << "\n";
    itr++;
  } // end while
  _map_lock.release_read();

  return(result.take_string());
}

FairBufferQueue * 
FairBuffer::request_queue() 
{
  FairBufferQueue* q = 0;
  _pool_lock.acquire_write();

  if(_queue_pool->size() == 0){
    _queue_pool->push_back(new FairBufferQueue(_capacity, _quantum, _max_burst));
    _creates++;
  } // end if

  // check out queue and remove from queue pool
  q = _queue_pool->back();
  _queue_pool->pop_back();
  _pool_lock.release_write();
  q->reset(_quantum, _max_burst);

  return(q);
}

void 
FairBuffer::release_queue(FairBufferQueue* q)
{
  _pool_lock.acquire_write();    
  _queue_pool->push_back(q);
  _pool_lock.release_write();
}

void 
FairBuffer::add_handlers()
{
  add_read_handler("reset", read_handler, (void*)H_RESET);
  add_read_handler("queue_creates", read_handler, (void*)H_CREATES);
  add_read_handler("queue_deletes", read_handler, (void*)H_DELETES);
  add_read_handler("drops", read_handler, (void*)H_DROPS);
  add_read_handler("byte_drops", read_handler, (void*)H_BYTEDROPS);
  add_read_handler("capacity", read_handler, (void*)H_CAPACITY);
  add_read_handler("list_queues", read_handler, (void*)H_LIST_QUEUE);
  add_read_handler("scheduler_active", read_handler, H_SCHEDULER_ACTIVE);
  add_read_handler("aggregator_active", read_handler, H_AGGREGATOR_ACTIVE);
  add_read_handler("max_delay", read_handler, H_MAX_DELAY);
  add_write_handler("scheduler_active", write_handler, H_SCHEDULER_ACTIVE);
  add_write_handler("aggregator_active", write_handler, H_AGGREGATOR_ACTIVE);
  add_write_handler("max_delay", write_handler, H_MAX_DELAY);
}

String 
FairBuffer::read_handler(Element *e, void *thunk)
{
  FairBuffer *c = (FairBuffer *)e;
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
  case H_BYTEDROPS:
    return(String(c->bdrops()) + "\n");
  case H_CAPACITY:
    return(String(c->capacity()) + "\n");
  case H_LIST_QUEUE:
    return(c->list_queues());
  case H_SCHEDULER_ACTIVE:
    return(String(c->_scheduler_active) + "\n");
  case H_MAX_DELAY:
    return(String(c->_max_delay) + "\n");
  case H_AGGREGATOR_ACTIVE:
    return(String(c->_aggregator_active) + "\n");
  default:
    return "<error>\n";
  }
}

int FairBuffer::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) 
{
	FairBuffer *d = (FairBuffer *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
		case H_SCHEDULER_ACTIVE: {
			bool flag;
			if (!cp_bool(s, &flag))
				return errh->error("parameter must be boolean");
			d->_scheduler_active = flag;
			break;
		}
		case H_AGGREGATOR_ACTIVE: {
			bool flag;
			if (!cp_bool(s, &flag))
				return errh->error("parameter must be boolean");
			d->_aggregator_active = flag;
			break;
		}
		case H_MAX_DELAY: {
			uint32_t max_delay;
			if (!cp_unsigned(s, &max_delay))
				return errh->error("parameter must be a positive integer");
			d->_max_delay = max_delay;
			break;
		}
	}
	return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue ARPTableMulti LinkTableMulti)
EXPORT_ELEMENT(FairBuffer)
