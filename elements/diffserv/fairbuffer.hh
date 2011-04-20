#ifndef FAIRBUFFER_HH
#define FAIRBUFFER_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include <click/hashtable.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/task.hh>
#include <click/timer.hh>
#include <elements/standard/simplequeue.hh>
CLICK_DECLS

/*
 * =c
 * FairBuffer([ETHTYPE, CAPACITY, QUANTUM, MAX_BURST, MIN_BURST, DELAY, LT, ARP])
 * =s ethernet
 * concatenates multiple MAC PDUs and implements the ADRR scheduling policy
 * =io
 * one output, one input
 * =d
 * This element expects Ethernet II frames. This element concatenates several 
 * MAC Service Data Units (MSDUs) to form the data payload of a larger MAC 
 * Protocol Data Unit (MPDU). The ethernet type for aggregate MSDUs is set to 
 * ETHTYPE. Incoming MAC frames are classified according to the destination 
 * address. Each flows is fed to a different queue holding up 
 * to CAPACITY frames. The default for CAPACITY is 1000. For each queue, an 
 * Aggregated-MSDU is generated when either DELAY seconds passed or a A-MSDU 
 * long MIN_BURST bytes can be generated. The element does not produces A-MSDU 
 * longer than MAX_BURST. The default for MIN_BURST and MAX_BURST are 
 * respectively 60 and 1500 bytes. Is LT is specified MIN_BURST is dynamically 
 * compute according with the link's mmetric. Aggregation buffers are polled 
 * according with a Deficit Round Robin (DRR) policy.
 */

class FairBufferQueue;

class FairBuffer : public SimpleQueue { public:

    FairBuffer();
    ~FairBuffer();

    const char* class_name() const		{ return "FairBuffer"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char* processing() const		{ return PUSH_TO_PULL; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *);
    int initialize(ErrorHandler *);

    void push(int port, Packet *);
    Packet *pull(int port);

    void add_handlers();

    uint32_t creates() { return(_creates); } // queues created
    uint32_t deletes() { return(_deletes); } // queues deleted
    uint32_t drops() { return(_drops); } // dropped packets
    uint32_t bdrops() { return(_bdrops); } // bytes dropped

    uint32_t quantum() { return(_quantum); } 
    uint32_t capacity() { return(_capacity); } 
    uint32_t max_burst() { return(_max_burst); } 
    uint16_t et() { return(_et); } 
    uint32_t aggregator_active() { return(_aggregator_active); } 
    uint32_t scheduler_active() { return(_scheduler_active); } 

    String list_queues();

    bool run_task(Task *);
    void reset();

  protected:

    enum { TRASH_TRIGGER = 9 };
    enum { SLEEPINESS_TRIGGER = 9 };

    int _sleepiness;
    ActiveNotifier _empty_note;

    Task _task;
    Timer _timer;

    typedef HashTable<EtherAddress, FairBufferQueue*> FairTable;
    typedef FairTable::iterator TableItr;

    typedef HashTable<EtherAddress, Packet*> HeadTable;
    typedef HeadTable::iterator HeadItr;

    typedef Vector<FairBufferQueue*> QueuePool;
    typedef QueuePool::iterator PoolItr;

    FairTable* _fair_table;
    HeadTable* _head_table;
    QueuePool* _queue_pool;

    EtherAddress _next;
    uint32_t _quantum;

    uint32_t _creates; // number of queues created
    uint32_t _deletes; // number of queues deleted
    uint32_t _drops; // packets dropped because of full queue
    uint32_t _bdrops; // bytes dropped

    FairBufferQueue* request_queue();
    void release_queue(FairBufferQueue*);
    
    uint32_t compute_deficit(Packet*);

    static int write_handler(const String &, Element *, void *, ErrorHandler *);
    static String read_handler(Element *, void *);

    uint16_t _et;     // This protocol's ethertype

    class ARPTableMulti* _arp_table;
    class LinkTableMulti* _lt;

    bool _scheduler_active;
    bool _aggregator_active;

    uint32_t _max_burst;
    uint32_t _max_delay;

};

class FairBufferQueue {

  public:

    ReadWriteLock _queue_lock;
    Packet** _q;
    FairBuffer * _fair_buffer;

    uint32_t _capacity;
    uint32_t _deficit;
    uint32_t _size;
    uint32_t _bsize;
    uint32_t _drops;
    uint32_t _head;
    uint32_t _tail;
    uint32_t _trash;
    Timestamp _last_update;

    FairBufferQueue(FairBuffer *fair_buffer) : _fair_buffer(fair_buffer) {
      _capacity = _fair_buffer->capacity();
      _q = new Packet*[_capacity];
      reset();
    }

    ~FairBufferQueue() {
      _queue_lock.acquire_write();
      for(uint32_t i = 0; i < _capacity; i++){
        if(_q[i]) {
          _q[i]->kill();
        }
      } 
      delete[] _q;
      _queue_lock.release_write();
    }  

    void reset() {
      _size = 0;
      _bsize = 0;
      _drops = 0;
      _head = 0;
      _tail = 0;
      _deficit = 0;
      _trash = 0;
      _last_update = Timestamp(0);
    }

    Packet* pull() {
      Packet* p = 0;
      _queue_lock.acquire_write();
      if(_head != _tail){
        _head = (_head+1)%_capacity;
        p = _q[_head];
        _q[_head] = 0;
        _size--;
        _bsize -= p->length();
      }
      _queue_lock.release_write();
      return(p);
    }

    bool push(Packet* p) {
      bool result = false;
      _queue_lock.acquire_write();
      if((_tail+1)%_capacity != _head){
        _tail++;
        _tail %= _capacity;
        _q[_tail] = p;
        _size++;
        _bsize += p->length();
        result = true;
      } else {
        _drops++;
      }
      _queue_lock.release_write();
      return result;
    }

    Packet* aggregate() {

      if (!ready()) {
        return 0;
      }

      if (!_fair_buffer->aggregator_active()) {
        return pull();
      }

      WritablePacket *wp = pull()->uniqueify();

      if (top() && (wp->length() + top()->length() < _fair_buffer->max_burst())) {

        click_ether *e = (click_ether *)wp->data();
        uint16_t ether_type = e->ether_type;
        uint16_t length = htons(wp->length() - sizeof(struct click_ether));

        EtherAddress *dst = new EtherAddress(e->ether_dhost);
        EtherAddress *src = new EtherAddress(e->ether_shost);

        uint16_t ether_type_pack = htons(_fair_buffer->et());

        wp = wp->push(4);

        memcpy(wp->data(), dst->data(), 6);
        memcpy(wp->data() + 6, src->data(), 6);
        memcpy(wp->data() + 12, &ether_type_pack, 2);	

        memcpy(wp->data() + 14, &length, 2);
        memcpy(wp->data() + 16, &ether_type, 2);

        do {

          uint32_t append = top()->length() - sizeof(struct click_ether);
          wp = wp->put(append+4);

          Packet* q = pull();

          click_ether *qe = (click_ether *)q->data();
          uint16_t ether_type = qe->ether_type;
          uint16_t length = htons(q->length() - sizeof(struct click_ether));

          memcpy(wp->data() + (wp->length()-append-4), &length, 2);
          memcpy(wp->data() + (wp->length()-append-2), &ether_type, 2);
          memcpy(wp->data() + (wp->length()-append), q->data()+sizeof(struct click_ether), append);

          q->kill();

        } while (top() && (wp->length() + top()->length() < _fair_buffer->max_burst()));

      }

      return wp;

    }

    bool ready() {
      if (!top()) {
        return false;
      }
      if (!_fair_buffer->aggregator_active()) {
        return true;
      }
      if (top()->timestamp_anno() <= Timestamp::now()) {
        return true;
      }
      if (_bsize >= _fair_buffer->max_burst()) {
        return true;
      }
      return false;
    }

    const Packet* top() {
      Packet* p = 0;
      _queue_lock.acquire_write();
      if(_head != _tail){
        p = _q[(_head+1)%_capacity];
      }
      _queue_lock.release_write();
      return(p);
    }

};

CLICK_ENDDECLS
#endif
