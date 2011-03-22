#ifndef CLICK_MINSTREL_HH
#define CLICK_MINSTREL_HH
#include <click/element.hh>
#include <click/dequeue.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <elements/wifi/bitrate.hh>
CLICK_DECLS

/*
 * =c
 * Minstrel([, I<KEYWORDS>])
 * =s Wifi
 * Minstrel wireless bit-rate selection algorithm
 * =a SetTXRate, FilterTX
 */

class Minstrel : public Element { public:

	Minstrel();
	~Minstrel();

	const char *class_name() const		{ return "Minstrel"; }
	const char *port_count() const		{ return "2/0-2"; }
	const char *processing() const		{ return "ah/a"; }
	const char *flow_code() const		{ return "#/#"; }

	int initialize(ErrorHandler *);
	int configure(Vector<String> &, ErrorHandler *);
	void run_timer(Timer *);

	void push (int, Packet *);
	Packet *pull(int);

	/* handler stuff */
	void add_handlers();
	String print_rates();

	uint32_t get_retry_count(uint32_t, uint32_t);
	void assign_rate(Packet *);
	void process_feedback(Packet *);

private:

	struct DstInfo {
	public:
		EtherAddress _eth;
		Vector<int> _rates;
		Vector<int> _success;
		Vector<int> _attempts;
		Vector<int> _last_success;
		Vector<int> _last_attempts;
		Vector<int> _hist_success;
		Vector<int> _hist_attempts;
		Vector<int> _cur_prob;
		Vector<int> _cur_tp;
		Vector<int> _probability;
		Vector<int> _retry_count;
		Vector<int> _sample_limit;
		int packet_count;
		int sample_count;
		int max_tp_rate;
		int max_tp_rate2;
		int max_prob_rate;
		DstInfo() {
		}
		DstInfo(EtherAddress eth, Vector<int> rates) {
			_eth = eth;
			_rates = rates;
			_success = Vector<int>(_rates.size(), 0);
			_attempts = Vector<int>(_rates.size(), 0);
			_last_success = Vector<int>(_rates.size(), 0);
			_last_attempts = Vector<int>(_rates.size(), 0);
			_hist_success = Vector<int>(_rates.size(), 0);
			_hist_attempts = Vector<int>(_rates.size(), 0);
			_cur_prob = Vector<int>(_rates.size(), 0);
			_cur_tp = Vector<int>(_rates.size(), 0);
			_probability = Vector<int>(_rates.size(), 0);
			_retry_count = Vector<int>(_rates.size(), 1);
			_sample_limit = Vector<int>(_rates.size(), -1);
			packet_count = 0;
			sample_count = 0;
			max_tp_rate = 0;
			max_tp_rate2 = 0;
			max_prob_rate = 0;
		}

		int get_next_sample() {
			return click_random(1, _rates.size() - 1);
		}

		int rate_index(int rate) {
			int ndx = 0;
			for (int x = 0; x < _rates.size(); x++) {
				if (rate == _rates[x]) {
					ndx = x;
					break;
				}
			}
			return (ndx == _rates.size()) ? -1 : ndx;
		}
		void add_result(int rate, int tries, int success) {
			int ndx = rate_index(rate);
			if (ndx >= 0) {
				_success[ndx] += success;
				_attempts[ndx] += tries;
			}
		}
	};

	typedef HashMap<EtherAddress, DstInfo> NeighborTable;
	typedef NeighborTable::iterator NeighborIter;

	NeighborTable _neighbors;
	AvailableRates *_rtable;
	Timer _timer;

	bool _active;
	bool _mrr;
	unsigned _offset;
	unsigned _period;
	unsigned _ewma_level;
	unsigned _lookaround_rate;
	bool _debug;

	static int write_handler(const String &, Element *, void *, ErrorHandler *);
	static String read_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif

