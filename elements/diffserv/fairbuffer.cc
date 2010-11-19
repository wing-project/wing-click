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
#include <click/error.hh>
#include "fairbuffer.hh"
#include <click/confparse.hh>
#include <clicknet/ether.h>
#include <elements/wing/arptablemulti.hh>
#include <elements/wing/linktablemulti.hh>
CLICK_DECLS

enum { H_RESET, H_DROPS, H_BYTEDROPS, H_CREATES, H_DELETES, H_CAPACITY, H_LIST_QUEUE };

FairBuffer::FairBuffer()
{
  _fair_table = new FairTable();
  _queue_pool = new QueuePool();
  reset();
}

FairBuffer::~FairBuffer()
{
  _map_lock.acquire_write();
  TableItr itr = _fair_table->begin();
  while(itr != _fair_table->end()){
    release_queue(itr.value());
    itr++;
  } // end while
  _fair_table->clear();
  delete _fair_table;
  clean_pool();
  _queue_pool->clear();
  delete _queue_pool;
  _map_lock.release_write();
}

void 
FairBuffer::clean_pool()
{
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
  _capacity=50;
  _quantum=1500;
  _scaling=1500;

  int res = cp_va_kparse(conf, this, errh,
			"CAPACITY", cpkP, cpUnsigned, &_capacity,
			"QUANTUM", cpkP, cpUnsigned, &_quantum,
			"SCALING", 0, cpInteger, &_scaling,
 			"LT", 0, cpElement, &_link_table, 
			"ARP", 0, cpElement, &_arp_table,
			cpEnd);

  if (_link_table && _link_table->cast("LinkTableMulti") == 0) {
    return errh->error("LT element is not a LinkTableMulti");
  }

  if (_arp_table && _arp_table->cast("ARPTableMulti") == 0) {
    return errh->error("ARP element is not a ARPTableMulti");
  }

  _full_note.initialize(Notifier::FULL_NOTIFIER, router());
  _full_note.set_active(true, false);
  _empty_note.initialize(Notifier::EMPTY_NOTIFIER, router());

  return res;
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
  } // end if 

  // push packet on queue or fail
  if(q->push(p)){
    _empty_note.wake(); 
    if (q->size() == q->capacity()) {
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

uint32_t 
FairBuffer::compute_deficit(const Packet* p)
{
  if (!p)
    return 0;
  if (_link_table) {
    IPAddress to = _arp_table->reverse_lookup(_next);
    uint32_t metric = _link_table->get_host_metric_from_me(to);
    if (!metric)
      return 0;
    return (p->length() * metric) / _scaling;
  } else {
    return p->length();
  }
}

Packet * 
FairBuffer::pull(int)
{
    _map_lock.acquire_write();
    int n = _fair_table->size();
    bool trash = false;
    // Look at each input once, starting at the *same*
    // one we left off on last time.
    for (int j = 0; j < n; j++) {
        TableItr active = _fair_table->find(_next);
        EtherAddress key = active.key();
        FairBufferQueue* queue = active.value();
	_sleepiness = 0;
	_full_note.wake();
	if (!queue->top()) {
            queue->deficit = 0;
            if (++queue->trash == TRASH_TRIGGER) {
              trash = true;
            }
        } else if (compute_deficit(queue->top()) <= queue->deficit) {
	    queue->deficit -= compute_deficit(queue->top());
	    return queue->pull();
	} else {
            queue->trash = 0;
        }
        active++;
        if (trash) {
            release_queue(queue);
            _fair_table->erase(key);
        } 
        if (_fair_table->empty()) {
            _next = EtherAddress();
            break;
        }
	if (active == _fair_table->end()) {
            active = _fair_table->begin();
    	}
        _next = active.key();
	active.value()->deficit += _quantum;
    }
    if (_fair_table->empty() && (++_sleepiness == SLEEPINESS_TRIGGER)) {
        _empty_note.sleep();
    }
    _map_lock.release_write();
    return 0;
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
	   << itr.value()->capacity() << "," 
	   << itr.value()->size() << "," 
	   << itr.value()->bsize() << "\n";
    itr++;
  } // end while
  _map_lock.release_read();

  return(result.take_string());
}

FairBufferQueue * 
FairBuffer::request_queue() 
{
  FairBufferQueue* q = 0;
  _pool_lock.acquire_write();  if(!_fair_table->empty()) 
    _next = _fair_table->begin().key();
  else
    _next = EtherAddress();

  if(_queue_pool->size() == 0){
    _queue_pool->push_back(new FairBufferQueue(_capacity));
    _creates++;
  } // end if

  // check out queue and remove from queue pool
  q = _queue_pool->back();
  _queue_pool->pop_back();
  _pool_lock.release_write();

  q->deficit = _quantum;
  q->trash = 0;

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
  default:
    return "<error>\n";
  }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(NotifierQueue LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(FairBuffer)    
