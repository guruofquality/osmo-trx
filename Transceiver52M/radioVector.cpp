/*
 * Written by Thomas Tsou <ttsou@vt.edu>
 * Based on code by Harvind S Samra <hssamra@kestrelsp.com>
 *
 * Copyright 2011 Free Software Foundation, Inc.
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

#include "radioVector.h"

radioVector::radioVector(const signalVector& wVector, GSM::Time& wTime)
	: signalVector(wVector), mTime(wTime)
{
}

radioVector::radioVector(size_t size, GSM::Time& wTime)
	: signalVector(size), mTime(wTime)
{
}

radioVector::radioVector(size_t size, size_t start, GSM::Time& wTime)
	: signalVector(size, start), mTime(wTime)
{
}

GSM::Time radioVector::getTime() const
{
	return mTime;
}

void radioVector::setTime(const GSM::Time& wTime)
{
	mTime = wTime;
}

bool radioVector::operator>(const radioVector& other) const
{
	return mTime > other.mTime;
}

mimoVector::mimoVector(size_t size, size_t chans, GSM::Time& wTime)
	: vectors(chans, NULL), mTime(wTime)
{
	for (size_t i = 0; i < vectors.size(); i++)
		vectors[i] = new signalVector(size);
}

mimoVector::mimoVector(size_t chans, GSM::Time& wTime)
	: vectors(chans, NULL), mTime(wTime)
{
}

mimoVector::~mimoVector()
{
	for (size_t i = 0; i < vectors.size(); i++) {
		delete vectors[i];
		vectors[i] = NULL;
	}
}

GSM::Time mimoVector::getTime() const
{
	return mTime;
}

void mimoVector::setTime(const GSM::Time& wTime)
{
	mTime = wTime;
}

bool mimoVector::operator>(const mimoVector& other) const
{
	return mTime > other.mTime;
}

signalVector *mimoVector::getVector(size_t chan)
{
	if (chan >= vectors.size())
		return NULL;

	return vectors[chan];
}

bool mimoVector::setVector(signalVector *vector, size_t chan)
{
	if (chan >= vectors.size())
		return false;

	delete vectors[chan];
	vectors[chan] = vector;
}

noiseVector::noiseVector(size_t n)
{
	this->resize(n);
	it = this->begin();
}

float noiseVector::avg()
{
	float val = 0.0;

	for (int i = 0; i < size(); i++)
		val += (*this)[i];

	return val / (float) size();
}

bool noiseVector::insert(float val)
{
	if (!size())
		return false;

	if (it == this->end())
		it = this->begin();

	*it++ = val;

	return true;
}

GSM::Time VectorQueue::nextTime() const
{
	GSM::Time retVal;
	mLock.lock();

	while (mQ.size()==0)
		mWriteSignal.wait(mLock);

	retVal = mQ.top()->getTime();
	mLock.unlock();

	return retVal;
}

radioVector* VectorQueue::getStaleBurst(const GSM::Time& targTime)
{
	mLock.lock();
	if ((mQ.size()==0)) {
		mLock.unlock();
		return NULL;
	}

	if (mQ.top()->getTime() < targTime) {
		radioVector* retVal = mQ.top();
		mQ.pop();
		mLock.unlock();
		return retVal;
	}
	mLock.unlock();

	return NULL;
}

radioVector* VectorQueue::getCurrentBurst(const GSM::Time& targTime)
{
	mLock.lock();
	if ((mQ.size()==0)) {
		mLock.unlock();
		return NULL;
	}

	if (mQ.top()->getTime() == targTime) {
		radioVector* retVal = mQ.top();
		mQ.pop();
		mLock.unlock();
		return retVal;
	}
	mLock.unlock();

	return NULL;
}
