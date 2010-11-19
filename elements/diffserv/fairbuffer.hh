#ifndef FAIRBUFFER_HH
#define FAIRBUFFER_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include <click/hashtable.hh>
#include <click/etheraddress.hh>
#include <elements/standard/notifierqueue.hh>
CLICK_DECLS

/*
 * =c
 * FairBuffer([CAPACITY, QUANTUM])
 * =s scheduling
 * deficit Round Robin scheduling among outgoing links.
 * =io
 * one output, one input
 * =d
 * Incoming packets are classified according to their Ethernet destination
 * address and fed to a buffer holding up to CAPACITY packets. Buffers are
 * polled according to a Deficit Round Robin policy. The default value for
 * QUANTUM is 500 bytes.
 *
 */

class FairBufferQueue {
 
  public:

    FairBufferQueue(uint32_t capacity) : _capacity(capacity) {
      _q = new Packet*[_capacity];
      _head = 0;
      _tail = 0;
      _size = 0;
      _bsize = 0;
      _drops = 0;
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

    const Packet* top() {
      Packet* p = 0;
      _queue_lock.acquire_write();
      if(_head != _tail){
        p = _q[(_head+1)%_capacity];
      }
      _queue_lock.release_write();
      return(p);
    }

    inline uint32_t capacity() { return(_capacity); }
    inline uint32_t size() { return(_size); }
    inline uint32_t bsize() { return(_bsize); }
    inline uint32_t drops() { return(_drops); }

    unsigned deficit;
    unsigned trash;

  protected:

    ReadWriteLock _queue_lock;
    Packet** _q;

    uint32_t _capacity;
    uint32_t _size;
    uint32_t _bsize;
    uint32_t _drops;
    uint32_t _head;
    uint32_t _tail;

};

class FairBuffer : public NotifierQueue { public:

    FairBuffer();
    ~FairBuffer();

    const char* class_name() const		{ return "FairBuffer"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char* processing() const		{ return PUSH_TO_PULL; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *);

    void push(int port, Packet *);
    Packet *pull(int port);

    void add_handlers();

    uint32_t creates() { return(_creates); } // queues created
    uint32_t deletes() { return(_deletes); } // queues deleted
    uint32_t drops() { return(_drops); } // dropped packets
    uint32_t bdrops() { return(_bdrops); } // bytes dropped

    uint32_t capacity() { return(_capacity); } // aggr. queues size

    String list_queues();

    void reset();

  protected:

    enum { SLEEPINESS_TRIGGER = 9 };
    enum { TRASH_TRIGGER = 9 };

    int _sleepiness;
    ActiveNotifier _full_note;

    typedef HashTable<EtherAddress, FairBufferQueue*> FairTable;
    typedef FairTable::const_iterator TableItr;

    typedef Vector<FairBufferQueue*> QueuePool;
    typedef QueuePool::iterator PoolItr;

    ReadWriteLock _map_lock;
    ReadWriteLock _pool_lock;
    FairTable* _fair_table;
    QueuePool* _queue_pool;

    EtherAddress _next;
    uint32_t _quantum;
    uint32_t _scaling;

    uint32_t _creates; // number of queues created
    uint32_t _deletes; // number of queues deleted
    uint32_t _drops; // packets dropped because of full queue
    uint32_t _bdrops; // bytes dropped

    FairBufferQueue* request_queue();
    void release_queue(FairBufferQueue*);
    
    uint32_t compute_deficit(const Packet*);

    void clean_pool();
    static String read_handler(Element *, void *);

    class ARPTableMulti* _arp_table;
    class LinkTableMulti* _link_table;

};

CLICK_ENDDECLS
#endif
