#ifndef CLICK_SAPACKET_HH
#define CLICK_SAPACKET_HH
CLICK_DECLS

inline uint8_t sa_keystream(uint8_t key, uint8_t msg) 
{
  return key ^ msg;
}

inline uint32_t sa_sensor_key(uint32_t master, uint8_t sensor) 
{
  click_srandom(master ^ sensor);
  return click_random();  
}

inline uint32_t sa_enc(uint32_t value, uint32_t keystream) 
{
  return (value + keystream) % (CLICK_RAND_MAX);
}

inline uint32_t sa_dec(uint32_t value, uint32_t keystream) 
{
  return (value - keystream) % (CLICK_RAND_MAX);
}

inline uint32_t sa_add_ciphers(uint32_t c1, uint32_t c2) 
{
  return (c1 + c2) % (CLICK_RAND_MAX);
}

inline uint32_t sa_add_keystreams(uint32_t k1, uint32_t k2) 
{
  return (k1 + k2) % (CLICK_RAND_MAX);
}

enum sapacket_type { 
  SA_PT_PROBE = 0x10,
  SA_PT_SINK = 0x20,
  SA_PT_PACK_N = 0x30,
  SA_PT_PACK_P = 0x31,
};

static const uint8_t _version = 0x0c;

CLICK_PACKED_STRUCTURE(
struct sapacket {,
  private:
	uint8_t _version; /* protocol version */
	uint8_t _type;    /* protocol type */
	uint8_t _msg;     /* overaly application */
	uint8_t _sensors;
	uint32_t _avg;
	uint32_t _var;
  public:
	uint8_t version() { return _version; }
	uint8_t type() { return _type; }
	uint8_t sensors() { return _sensors; }
	uint8_t msg() { return _msg; }
	uint32_t avg() { return ntohl(_avg); }
	uint32_t var() { return ntohl(_var); }
	void set_version(uint8_t version) { _version = version; }
	void set_type(uint8_t type) { _type = type; }
	void set_sensors(uint8_t sensors) { _sensors = sensors; }
	void set_msg(uint8_t msg) { _msg = msg; }
	void set_avg(uint32_t avg) { _avg = htonl(avg); }
	void set_var(uint32_t var) { _var = htonl(var); }
	/* packet length functions */
	static size_t len(int sensors) {
                unsigned size = (sensors == 0) ? sizeof(struct sapacket) : sizeof(struct sapacket) + (((int)((sensors - 1) / 4)) * 4 + 4) * sizeof(uint8_t);
		return size;
	}
	uint8_t get_sensor(int id) {
		uint8_t *ndx = (uint8_t *) (this+1);
		ndx += id;
		return ndx[0];
	}	
	void set_sensor(int id, uint8_t sensor) {
		uint8_t *ndx = (uint8_t *) (this+1);
		ndx += id;
		ndx[0] = sensor;
	}
});

CLICK_ENDDECLS
#endif /* CLICK_SAPACKET_HH */
