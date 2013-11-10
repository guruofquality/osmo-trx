/*
 * Radio device interface with sample rate conversion
 * Written by Thomas Tsou <tom@tsou.cc>
 *
 * Copyright 2011, 2012, 2013 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * See the COPYING file in the main directory for details.
 */

#include <radioInterface.h>
#include <Logger.h>

#include "Resampler.h"

extern "C" {
#include "convert.h"
}

/* Resampling parameters for 64 MHz clocking */
#define RESAMP_64M_INRATE			65
#define RESAMP_64M_OUTRATE			96

/* Downlink block size */
#define CHUNK					625

/* Universal resampling parameters */
#define NUMCHUNKS				24

/*
 * Resampling filter bandwidth scaling factor
 *   This narrows the filter cutoff relative to the output bandwidth
 *   of the polyphase resampler. At 4 samples-per-symbol using the
 *   2 pulse Laurent GMSK approximation gives us below 0.5 degrees
 *   RMS phase error at the resampler output.
 */
#define RESAMP_TX4_FILTER		0.45

static int resamp_inrate = 0;
static int resamp_inchunk = 0;
static int resamp_outrate = 0;
static int resamp_outchunk = 0;

RadioInterfaceDiversity::RadioInterfaceDiversity(RadioDevice *wRadio,
						 size_t chans, size_t diversity,
						 size_t sps, int wReceiveOffset,
						 GSM::Time wStartTime)
	: RadioInterface(wRadio, wReceiveOffset,
			 sps, chans, diversity, wStartTime),
	  outerRecvBuffer(NULL)
{
}

RadioInterfaceDiversity::~RadioInterfaceDiversity()
{
	close();
}

void RadioInterfaceDiversity::close()
{
	delete outerRecvBuffer;

	outerRecvBuffer = NULL;

	for (size_t i = 0; i < dnsamplers.size(); i++) {
		delete dnsamplers[i];
		dnsamplers[i] = NULL;
	}

	if (recvBuffer.size())
		recvBuffer[0] = NULL;

	RadioInterface::close();
}

/* Initialize I/O specific objects */
bool RadioInterfaceDiversity::init(int type)
{
	int inner_rx_len, outer_rx_len;
	int tx_len;

	if ((mMIMO != 2) || (mChans != 2)) {
		LOG(ALERT) << "Unsupported channel configuration " << mChans;
		return false;
	}

	close();

	/* Resize for channel combination */
	sendBuffer.resize(mChans);
	recvBuffer.resize(mChans * mMIMO);
	convertSendBuffer.resize(mChans);
	convertRecvBuffer.resize(mChans);
	mReceiveFIFO.resize(mChans);
	dnsamplers.resize(mChans);

	/* Inner and outer rates */
	resamp_inrate = RESAMP_64M_INRATE;
	resamp_outrate = RESAMP_64M_OUTRATE;
	resamp_inchunk = resamp_inrate * 4;
	resamp_outchunk = resamp_outrate * 4;

	/* Buffer lengths */
	outer_rx_len = resamp_outchunk;
	inner_rx_len = NUMCHUNKS * resamp_inchunk / mSPSTx;
	tx_len = CHUNK * mSPSTx;

	/* Inside buffer must hold at least 2 bursts */
	if (inner_rx_len < 157 * mSPSRx * 2) {
		LOG(ALERT) << "Invalid inner Rx buffer size " << inner_rx_len;
		return false;
	}

	for (size_t i = 0; i < mChans; i++) {
		dnsamplers[i] = new Resampler(resamp_inrate, resamp_outrate);
		if (!dnsamplers[i]->init()) {
			LOG(ALERT) << "Rx resampler failed to initialize";
			return false;
		}

		/* Full rate float and integer outer receive buffers */
		convertRecvBuffer[i] = new short[outer_rx_len * 2];

		/* GSM rate inner receive buffers*/
		for (size_t n = 0; i < mMIMO; n++) {
			recvBuffer[mMIMO * i + n] =
				new signalVector(inner_rx_len);
		}

		/* Send buffers (not-resampled) */
		sendBuffer[i] = new signalVector(tx_len);
		convertSendBuffer[i] = new short[tx_len * 2];
	}

	outerRecvBuffer = new signalVector(outer_rx_len, dnsamplers[0]->len());

	return true;
}

/* Receive a timestamped chunk from the device */
void RadioInterfaceDiversity::pullBuffer()
{
	bool local_underrun;
	int rc, num_recv;

	if (recvCursor > recvBuffer[0]->size() - resamp_inchunk)
		return;

	/* Outer buffer access size is fixed */
	num_recv = mRadio->readSamples(convertRecvBuffer,
				       resamp_outchunk,
				       &overrun,
				       readTimestamp,
				       &local_underrun);
	if (num_recv != resamp_outchunk) {
		LOG(ALERT) << "Receive error " << num_recv;
		return;
	}

	for (size_t i = 0; i < mChans; i++) {
		convert_short_float((float *) outerRecvBuffer->begin(),
			    convertRecvBuffer[i], 2 * resamp_outchunk);

		/* Frequency translation */

		/* Write to the end of the inner receive buffer */
		rc = dnsamplers[i]->rotate(
			(float *) outerRecvBuffer->begin(),
			resamp_outchunk,
			(float *) (recvBuffer[i]->begin() + recvCursor),
			resamp_inchunk);

		if (rc < 0) {
			LOG(ALERT) << "Sample rate downsampling error";
		}
	}

	underrun |= local_underrun;
	readTimestamp += (TIMESTAMP) resamp_outchunk;
	recvCursor += resamp_inchunk;
}
