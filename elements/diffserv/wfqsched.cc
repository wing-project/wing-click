/*
 * wfqsched.{cc,hh} 
 *
 * Nicola Arnoldi, Roberto Riggio
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
#include "wfqsched.hh"
#include <click/confparse.hh>
CLICK_DECLS

enum { H_WEIGHTS };

WFQSched::WFQSched()
    : _signals(0)
{
}

WFQSched::~WFQSched()
{
}

int
WFQSched::initialize(ErrorHandler *errh)
{
	_signals = new NotifierSignal[ninputs()]; 
	if (!_signals)
		return errh->error("out of memory!");
	for (int i = 0; i < ninputs(); i++) 
		_signals[i] = Notifier::upstream_empty_signal(this, i);
	return 0;
}

void
WFQSched::cleanup(CleanupStage)
{
    delete[] _signals;
    delete[] _weights;
}

int
WFQSched::configure(Vector<String> &conf, ErrorHandler *errh)
{	
	int before = errh->nerrors();
	if (conf.size() != ninputs())
		 errh->error("need %d arguments, one per input port", ninputs());
	_weights_sum = 0;
	_weights = new int[ninputs()];
	_extractions = new int[ninputs()];
	_skips = new int[ninputs()];
	for (int i = 0; i < conf.size(); i++) {
		int v;
		if (!cp_integer(conf[i], &v))
		    errh->error("argument %d should be a weight (integer)", i);
		else if (v <= 0)
		    errh->error("argument %d (weight) must be > 0", i);
		else {
			_weights_sum+=v;
			_weights[i]=v;
			_extractions[i]=0;
			_skips[i]=0;
		}    
    	}
	find_schedule();
	_next = 0;
	return (errh->nerrors() == before ? 0 : -1);
}

void
WFQSched::find_schedule()
{
	Vector<double> *v = new Vector<double>[_weights_sum];

	_schedule = new int[_weights_sum];
	int s=0;

	for (int i=0; i < ninputs();i++)
	{
		for (int k = 0; k < _weights[i]; k++)
		{
			v[s].push_back(((double)(k+1))/((double)_weights[i]));
			v[s].push_back((double)i);
			s++;
		}
	}
	
	for (int i = 0; i < _weights_sum-1; i++) {
		int min = i;
		for (int j = i+1; j < _weights_sum; j++)
			if (v[j].size() < v[min].size()) 
				min=j;
		v[min].swap(v[i]);
	}

	for (int i = 0; i < _weights_sum - 1; i++)
	{
		int min = i;
		for (int j = i + 1; j < _weights_sum; j++)
			if (v[j].size() < v[min].size()) min = j;
		v[min].swap(v[i]);
	}

	for (int i = 0; i < _weights_sum; i++) {
		_schedule[i] = (int) v[i].at(1);
		click_chatter("%{element} :: %s :: schedule", 
				this,
				__func__, 
				_schedule[i]);
	}
}

Packet *
WFQSched::pull(int)
{
	Packet *p = 0;
	for (int s = 0; s < ninputs(); s++) {
		int index = _schedule[_next];
		_next++;
		if (_next == _weights_sum) _next = 0;

		if (_signals[index] && (p = input(index).pull())) {
			_extractions[index]++;
			return p;
		} else {
			_skips[index]++;
		}	
	}
	return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WFQSched)

