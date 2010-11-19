#ifndef CLICK_HBHAGGREGATIONPOINT_HH
#define CLICK_HBHAGGREGATIONPOINT_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include "elements/standard/notifierqueue.hh"
#include "availablesensors.hh"
#include "sapacket.hh"
CLICK_DECLS

class HBHAggregationPointQueue {

  public:

    ReadWriteLock _queue_lock;
    Packet** _q;

    uint32_t _capacity;
    uint32_t _size;
    uint32_t _sensors;
    uint32_t _drops;
    uint32_t _head;
    uint32_t _tail;
    uint32_t _trash;
    Timestamp _last_touched;   

    HBHAggregationPointQueue(uint32_t capacity) : _capacity(capacity) {
      _q = new Packet*[_capacity];
      reset();
    }

    ~HBHAggregationPointQueue() {
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
      _sensors = 0;
      _drops = 0;
      _head = 0;
      _tail = 0;
      _trash = 0;
      _last_touched = Timestamp::now();
    }

    Packet* pull() {
      Packet* p = 0;
      _queue_lock.acquire_write();
      if(_head != _tail){
        _head = (_head+1)%_capacity;
        p = _q[_head];
        _sensors -= ((struct sapacket *) p->data())->sensors();
        _q[_head] = 0;
        _size--;
        _last_touched = Timestamp::now();
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
        _sensors += ((struct sapacket *) p->data())->sensors();
        _size++;
        _last_touched = Timestamp::now();
        result = true;
      } else {
        _drops++;
      }
      _queue_lock.release_write();
      return result;
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

    bool burst_ready(uint32_t jitter) {
      if (_size == 0) {
        return false;
      }
      return (_last_touched + Timestamp::make_msec(jitter) < Timestamp::now());
    }

};

class HBHAggregationPoint : public NotifierQueue { public:

    HBHAggregationPoint();
    ~HBHAggregationPoint();

    const char* class_name() const		{ return "HBHAggregationPoint"; }
    const char *port_count() const		{ return "-/1"; }
    const char* processing() const		{ return PUSH_TO_PULL; }
    void *cast(const char *);

    int configure(Vector<String> &conf, ErrorHandler *);

    void push(int port, Packet *);
    Packet *pull(int port);

    void add_handlers();

    uint32_t creates() { return(_creates); } // queues created
    uint32_t deletes() { return(_deletes); } // queues deleted
    uint32_t drops() { return(_drops); } // dropped packets
    uint32_t capacity() { return(_capacity); } // aggr. queues size

    void reset();

  protected:

    enum { SLEEPINESS_TRIGGER = 9 };
    enum { TRASH_TRIGGER = 9 };

    int _sleepiness;

    typedef HashTable<uint8_t, HBHAggregationPointQueue*> AppTable;
    typedef AppTable::const_iterator AppItr;

    typedef Vector<HBHAggregationPointQueue*> QueuePool;
    typedef QueuePool::iterator PoolItr;

    ReadWriteLock _map_lock;
    ReadWriteLock _pool_lock;
    AppTable* _inc_table;
    AppTable* _fwd_table;
    QueuePool* _queue_pool;

    uint8_t _next;

    uint32_t _creates; // number of queues created
    uint32_t _deletes; // number of queues deleted
    uint32_t _drops; // packets dropped because of full queue

    HBHAggregationPointQueue* request_queue();
    void release_queue(HBHAggregationPointQueue*);
    
    void clean_pool();
    static String read_handler(Element *, void *);

    uint32_t _jitter;
    bool _debug;

    class AvailableSensorsIndex *_as_table;

};

CLICK_ENDDECLS
#endif
