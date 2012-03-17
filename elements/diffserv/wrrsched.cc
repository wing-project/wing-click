/*
 * wrrsched.{cc,hh} 
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
#include "wrrsched.hh"
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS


enum { H_WEIGHTS };


WRRSched::WRRSched()
    : _signals(0)
{
}

WRRSched::~WRRSched()
{
}


int
WRRSched::initialize(ErrorHandler *errh)
{
	_signals = new NotifierSignal[ninputs()];
	if (!_signals)
		return errh->error("out of memory!");

	for (int i = 0; i < ninputs(); i++) 
		_signals[i] = Notifier::upstream_empty_signal(this, i);

	return 0;
}


void
WRRSched::cleanup(CleanupStage)
{
	delete[] _signals;
	delete[] _weights;
}

int
WRRSched::configure(Vector<String> &conf, ErrorHandler *errh)
{	
	int before = errh->nerrors();
	if (conf.size() != ninputs())
		 errh->error("need %d arguments, one per input port", ninputs());

	_weights = new int[ninputs()];

	for (int i = 0; i < conf.size(); i++) {
		int v;
		if (!cp_integer(conf[i], &v)) {
			errh->error("argument %d should be a weight (integer)", i);
		} else if (v <= 0) {
			errh->error("argument %d (weight) must be > 0", i);
		} else {
			_weights[i] = v;
		}    
	}
	
	return (errh->nerrors() == before ? 0 : -1);
}

Packet *
WRRSched::pull(int)
{
	Vector<int> *v = new Vector<int>();

	for (int i = 0; i < ninputs(); i++) {
		if (_signals[i]) {
			for (int j = 0; j < _weights[i]; j++) v->push_back(i);
		}
	}

	if (v->size() == 0) return 0;

	int range = v->size();
	int r = int(range * (rand() / (RAND_MAX + 1.0))); 
	int index = v->at(r);

	return _signals[index] ? input(index).pull() : 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(WRRSched)

