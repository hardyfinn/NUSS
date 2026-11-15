// Channel Index signalling shared by EFG, ETS and EGX (spec §4.11).
// A channel number travels as a voltage: vPer × channel (0.5 V per channel by default), 0 V = none.
#pragma once
#include "Common.hpp"

namespace ef {

// Sender. push() announces a channel (1..8); several pushes in one sample go out as a "flam":
// each index for 5 ms in turn, the last one staying high. Re-announcing the index already
// on the output drops to 0 V for a 1 ms gap first, so a receiver sees a new index.
struct ChannelIndexOut {
	float vPer = INDEX_V_DEFAULT;
	int cur = 0;            // channel on the output, 0 = none
	int afterGap = 0;
	float gapLeft = 0.f;
	float holdLeft = -1.f;  // < 0: hold until something new arrives
	int q[INDEX_QUEUE];
	int qn = 0;

	void reset() { cur = 0; afterGap = 0; gapLeft = 0.f; holdLeft = -1.f; qn = 0; }
	void push(int ch) {
		if (ch < 1 || ch > CH) return;
		if (qn < INDEX_QUEUE) q[qn++] = ch;
	}
	// The channel's activity ended: return to 0 V if it is the one showing and nothing else is due.
	void release(int ch) {
		if (cur == ch && holdLeft < 0.f && qn == 0 && gapLeft <= 0.f) cur = 0;
	}
	float volts() const { return cur * vPer; }
	int pop() {
		int v = q[0];
		for (int i = 1; i < qn; i++) q[i - 1] = q[i];
		qn--;
		return v;
	}
	float process(float dt) {
		if (gapLeft > 0.f) {
			gapLeft -= dt;
			if (gapLeft > 0.f) return 0.f;
			cur = afterGap;
			holdLeft = qn ? INDEX_FLAM_S : -1.f;
			return volts();
		}
		if (holdLeft > 0.f) {
			holdLeft -= dt;
			if (holdLeft <= 0.f) holdLeft = -1.f;
		}
		if (holdLeft < 0.f && qn > 0) {
			int nx = pop();
			if (nx == cur) {
				cur = 0;
				afterGap = nx;
				gapLeft = INDEX_GAP_S;
				return 0.f;
			}
			cur = nx;
			holdLeft = qn ? INDEX_FLAM_S : -1.f;
		}
		return volts();
	}
};

// Receiver: returns a channel (1..8) each time the incoming index changes to a non-zero value.
struct ChannelIndexIn {
	int last = 0;
	static int decode(float v, float vPer) {
		int i = (int) std::lround(v / vPer);
		return (i < 1 || i > CH) ? 0 : i;
	}
	int process(float v, float vPer) {
		int i = decode(v, vPer);
		int fired = (i != last && i > 0) ? i : 0;
		last = i;
		return fired;
	}
	void reset() { last = 0; }
};

} // namespace ef
