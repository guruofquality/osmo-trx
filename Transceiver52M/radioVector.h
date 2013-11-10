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

#ifndef RADIOVECTOR_H
#define RADIOVECTOR_H

#include "sigProcLib.h"
#include "GSMCommon.h"
#include "Interthread.h"

class radioVector : public signalVector {
public:
	radioVector(const signalVector& wVector, GSM::Time& wTime);
	radioVector(size_t size, GSM::Time& wTime);
	radioVector(size_t size, size_t start, GSM::Time& wTime);
	GSM::Time getTime() const;
	void setTime(const GSM::Time& wTime);
	bool operator>(const radioVector& other) const;

private:
	GSM::Time mTime;
};

class mimoVector {
public:
	mimoVector(size_t chans, GSM::Time& wTime);
	mimoVector(size_t size, size_t chans, GSM::Time& wTime);
	~mimoVector();

	GSM::Time getTime() const;
	void setTime(const GSM::Time& wTime);
	bool operator>(const mimoVector& other) const;

	signalVector *getVector(size_t chan);
	bool setVector(signalVector *vector, size_t chan);
	size_t chans() { return vectors.size(); }
private:
	std::vector<signalVector *> vectors;
	GSM::Time mTime;
};

class noiseVector : std::vector<float> {
public:
	noiseVector(size_t len = 0);
	bool insert(float val);
	float avg();

private:
	std::vector<float>::iterator it;
};

class VectorFIFO : public InterthreadQueue<mimoVector> { };

class VectorQueue : public InterthreadPriorityQueue<radioVector> {
public:
	GSM::Time nextTime() const;
	radioVector* getStaleBurst(const GSM::Time& targTime);
	radioVector* getCurrentBurst(const GSM::Time& targTime);
};

#endif /* RADIOVECTOR_H */
