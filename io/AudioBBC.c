/***********************************************************************************************************************
PicoMite MMBasic

AudioBBC.c

<COPYRIGHT HOLDERS>  Geoff Graham, Peter Mather
Copyright (c) 2021, <COPYRIGHT HOLDERS> All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
1.	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
	in the documentation and/or other materials provided with the distribution.
3.	The name MMBasic be used when referring to the interpreter in any documentation and promotional material and the original copyright message be displayed
	on the console at startup (additional copyright messages may be added).
4.	All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed
	by the <copyright holder>.
5.	Neither the name of the <copyright holder> nor the names of its contributors may be used to endorse or promote products derived from this software
	without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDERS> AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDERS> BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

************************************************************************************************************************/
/**
 * @file AudioBBC.c
 * @author Peter Mather
 * @brief The BBC Micro sound system behind PLAY BBC SOUND and PLAY BBC ENVELOPE
 */
/**
 * @cond
 * The following section will be excluded from the documentation.
 */
/*
 * The classic BBC Micro model: channels 1-3 are square-wave tones and
 * channel 0 is an LFSR noise source with the BBC's eight settings -
 * periodic or white, at three fixed rates or tuned to channel 1's tone.
 * Each channel has a 16-note queue (MM.INFO(BBC BUFFER) reports the free
 * slots, so a program can keep ahead of the engine without blocking);
 * the channel word's &1x bit flushes it and the &Sxx bits hold a note
 * until S other channels carry the same sync mark, which is how chords
 * start together.  ENVELOPE 1-16 gives the three-section pitch envelope
 * (auto-repeating, so vibrato, arpeggios and slides) and an ADSR
 * amplitude envelope, stepped at the authentic 100 Hz.  Pitch is the BBC
 * scale of 4 units per semitone with 89 = A4 = 440 Hz; durations are in
 * 20ths of a second and 255 means until further notice.
 *
 * This is a port of the Pico Computer 3's Fuzix sound engine (which also
 * carries a port of MMBasic's own PLAY SOUND synth, so the two have met
 * before).  The squares are band-limited with polyBLEP so a sustained
 * note holds a pure pitch at the top of the range instead of carrying the
 * aliasing shimmer a naive digital square picks up.
 *
 * Everything here is integer arithmetic, so it runs unchanged on the
 * RP2040.  The engine is one more filler for the audio swing buffers,
 * beside fillToneBuffer and fillSoundBuffer: fillBBCBuffer() writes
 * signed 16-bit stereo frames (both sides the same - the BBC was mono)
 * and Audio.c's iconvert/i2sconvert apply PLAY VOLUME and shape them for
 * the PWM, I2S or VS1053 output as they do for PLAY TONE.  It runs at
 * PWM_FREQ (44100), where PLAY TONE and PLAY SOUND run, so nothing in
 * the output stage changes with the source.
 *
 * Like PLAY TONE, the output is released when there is nothing left to
 * play: fillBBCBuffer sets playreadcomplete once every channel is idle
 * and every queue is empty, and audio_checks() then stops the audio.
 * The next PLAY BBC SOUND starts it again.
 *
 * RAM.  The engine's state - the four channels with their queues, the
 * sixteen envelopes, the noise generator - is one block of 608 bytes
 * (three 256-byte heap pages; it was 480 bytes in two pages when the
 * queues were 8 deep) taken from the MMBasic heap by the first PLAY BBC
 * command a program issues, so a program that never uses the engine pays
 * nothing for it.  The only permanent cost is the pointer.  The block
 * outlives the audio output: it is kept through the automatic stop and
 * through PLAY STOP, so envelope definitions survive them as they did on
 * the BBC, and it is released when the program's variables are - by
 * ClearRuntime, on RUN, NEW and program load - through BBCSoundRelease.
 * The engine is silenced and its queues flushed, envelopes kept, by
 * BBCSoundReset, which CloseAudio calls.
 *
 * Nothing here runs in an interrupt.  The filler is called from the
 * interpreter's idle loop (checkWAVinput), the same thread the commands
 * run in, so the queues need no locking, and the block is only ever
 * allocated from a command.
 */
#include <stdint.h>
#include <string.h>
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

#define BBC_QLEN 16	   /* notes per channel queue */
/* One bit set.  Periodic noise circulates a single bit round the fifteen
 * stages, so a register of ones with single-tap feedback would never change
 * and would be silent.  The chip clears its register when the noise control
 * is written, which is what start_note does for channel 0. */
#define NOISE_SEED 0x4000u
#define BBC_RATE 44100 /* PWM_FREQ: the pitch table shift and the noise clock assume it */

/* BBC pitch: the top 16 bits of the phase increments for pitches 240-287 at
 * 22050 Hz (the low 16 bits are worth 0.03 cents).  At 44100
 * inc(p) = (table[p % 48] << 16) >> (6 - p / 48); p is 0..255 so the shift is 1..6. */
static const uint16_t pinc48[48] = {
	0x2D37, 0x2DDF, 0x2E8A, 0x2F37, 0x2FE7, 0x3099, 0x314E, 0x3206,
	0x32C0, 0x337D, 0x343D, 0x3500, 0x35C5, 0x368D, 0x3758, 0x3826,
	0x38F7, 0x39CB, 0x3AA3, 0x3B7D, 0x3C5B, 0x3D3B, 0x3E1F, 0x3F07,
	0x3FF1, 0x40DF, 0x41D1, 0x42C6, 0x43BF, 0x44BB, 0x45BB, 0x46BE,
	0x47C6, 0x48D1, 0x49E0, 0x4AF3, 0x4C0A, 0x4D26, 0x4E45, 0x4F68,
	0x5090, 0x51BC, 0x52EC, 0x5421, 0x555A, 0x5698, 0x57DB, 0x5922,
};

struct bbcnote
{
	uint8_t amp;   /* 0-15 volume, or 0x80 | envelope number */
	uint8_t pitch;
	uint8_t dur;   /* 20ths of a second; 255 = forever */
	uint8_t sync;  /* 0-3 */
};

struct bbcchan
{
	/* queue */
	struct bbcnote q[BBC_QLEN];
	uint8_t qr, qw;
	/* playing note */
	uint8_t active;
	uint8_t env;	/* envelope number or 0 */
	uint8_t pitch;	/* base pitch of the note */
	int16_t dur_cs; /* remaining, -1 = forever */
	int16_t level;	/* current amplitude 0..126 */
	uint32_t phase, inc;
	/* envelope runtime */
	uint8_t esec;	/* pitch section 0-2, 3 = done */
	uint8_t ecount; /* steps left in section */
	int16_t poff;	/* accumulated pitch offset */
	uint8_t ephase; /* 0 attack 1 decay 2 sustain 3 release */
	uint8_t tctr;	/* envelope step countdown (cs) */
};

/* The whole of the engine's state: one heap block, see the note above. */
struct bbcstate
{
	struct bbcchan ch[4];
	uint8_t envs[17][13]; /* T,PI1-3,PN1-3,AA,AD,AS,AR,ALA,ALD; row 0 unused */
	uint32_t noise_lfsr;
	uint8_t noise_ctr;
	uint32_t noise_phase; /* pitch 3 and 7: clocked from channel 1 */
	uint16_t cs_acc; /* 100 Hz tick accumulator */
};

static struct bbcstate *bbc = NULL;

/* Command context only: GetMemory errors out of the command if the heap
 * is exhausted, which is the right answer to a PLAY with no room.  It
 * zero-fills, so an idle engine with no envelopes is the initial state. */
static struct bbcstate *bbc_alloc(void)
{
	if (bbc == NULL)
	{
		bbc = GetMemory(sizeof(struct bbcstate));
		bbc->noise_lfsr = NOISE_SEED;
	}
	return bbc;
}

/* --- note/envelope engine --------------------------------------------------- */

static void set_inc(struct bbcchan *c)
{
	int p = c->pitch + (c->env ? c->poff : 0);
	if (p < 0)
		p = 0;
	if (p > 255)
		p = 255;
	c->inc = ((uint32_t)pinc48[p % 48] << 16) >> (6 - p / 48);
}

static void start_note(struct bbcchan *c, struct bbcnote *n)
{
	c->pitch = n->pitch;
	c->dur_cs = (n->dur == 255) ? -1 : (n->dur ? n->dur * 5 : 1);
	c->poff = 0;
	c->esec = 0;
	c->ecount = 0;
	c->ephase = 0;
	c->tctr = 0;
	if (n->amp & 0x80)
	{
		c->env = n->amp & 0x7F;
		if (c->env > 16)
			c->env = 16;
		c->level = 0;
		c->ecount = bbc->envs[c->env][4]; /* PN1 */
	}
	else
	{
		c->env = 0;
		c->level = (n->amp > 15 ? 15 : n->amp) * 8; /* 0..120 */
	}
	c->active = 1;
	set_inc(c);
	if (c == &bbc->ch[0])
	{
		/* The chip clears its shift register whenever the noise control is
		 * written, and periodic noise depends on it - see NOISE_SEED. */
		bbc->noise_lfsr = NOISE_SEED;
		bbc->noise_ctr = 0;
		bbc->noise_phase = 0;
	}
}

static void env_step(struct bbcchan *c)
{
	uint8_t *e = bbc->envs[c->env];
	/* pitch envelope: sections of PN1-3 steps of PI1-3 each */
	if (c->esec < 3)
	{
		while (c->esec < 3 && c->ecount == 0)
		{
			c->esec++;
			if (c->esec < 3)
				c->ecount = e[4 + c->esec];
			else if (!(e[0] & 0x80) && (e[4] | e[5] | e[6]))
			{
				/* auto-repeat the pitch envelope.  Only if it has any steps:
				 * with PN1-3 all zero this loop would otherwise never end,
				 * and it runs in the interpreter's idle loop where Ctrl-C
				 * cannot reach it (found on the first board test; the
				 * original in the PC3 kernel has the same hazard). */
				c->esec = 0;
				c->ecount = e[4];
				c->poff = 0;
			}
		}
		if (c->esec < 3 && c->ecount)
		{
			c->poff += (int8_t)e[1 + c->esec];
			c->ecount--;
			set_inc(c);
		}
	}
	/* amplitude ADSR: AA until ALA, AD until ALD, AS, then release */
	{
		int16_t lvl = c->level;
		int8_t ala = e[11] & 0x7F, ald = e[12] & 0x7F;
		switch (c->ephase)
		{
		case 0:
			lvl += (int8_t)e[7];
			if (lvl >= ala)
			{
				lvl = ala;
				c->ephase = 1;
			}
			break;
		case 1:
			lvl += (int8_t)e[8];
			if (lvl <= ald)
			{
				lvl = ald;
				c->ephase = 2;
			}
			break;
		case 2:
			lvl += (int8_t)e[9]; /* AS: 0 or negative */
			break;
		case 3:
			lvl += (int8_t)e[10]; /* AR: negative */
			break;
		}
		if (lvl < 0)
			lvl = 0;
		if (lvl > 126)
			lvl = 126;
		c->level = lvl;
		if (c->ephase == 3 && lvl == 0)
			c->active = 0;
	}
}

static void try_dequeue(void)
{
	int i, j, n;
	for (i = 0; i < 4; i++)
	{
		struct bbcchan *c = &bbc->ch[i];
		if (c->active || c->qr == c->qw)
			continue;
		struct bbcnote *hd = &c->q[c->qr % BBC_QLEN];
		if (hd->sync)
		{
			/* count idle channels whose head carries the same sync */
			n = 0;
			for (j = 0; j < 4; j++)
			{
				struct bbcchan *o = &bbc->ch[j];
				if (!o->active && o->qr != o->qw &&
					o->q[o->qr % BBC_QLEN].sync == hd->sync)
					n++;
			}
			if (n < hd->sync + 1)
				continue;
			/* release the whole group */
			for (j = 0; j < 4; j++)
			{
				struct bbcchan *o = &bbc->ch[j];
				if (!o->active && o->qr != o->qw &&
					o->q[o->qr % BBC_QLEN].sync == hd->sync)
				{
					start_note(o, &o->q[o->qr % BBC_QLEN]);
					o->qr++;
				}
			}
		}
		else
		{
			start_note(c, hd);
			c->qr++;
		}
	}
}

static void tick_100hz(void)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		struct bbcchan *c = &bbc->ch[i];
		if (!c->active)
			continue;
		if (c->env)
		{
			uint8_t t = bbc->envs[c->env][0] & 0x7F;
			if (t == 0)
				t = 1;
			if (++c->tctr >= t)
			{
				c->tctr = 0;
				env_step(c);
			}
		}
		if (c->dur_cs > 0 && --c->dur_cs == 0)
		{
			if (c->env && c->ephase < 3)
				c->ephase = 3; /* enter release */
			else
				c->active = 0;
		}
	}
	try_dequeue();
}

/* --- mixer: one swing buffer of signed 16-bit stereo frames ---------------- */

// Fill a swing buffer with mixed BBC channel samples as signed 16-bit PCM
// stereo pairs, both sides the same.  Returns the count of int16_t values
// written (always even).  Flags playreadcomplete when nothing is left to
// play, so audio_checks() releases the output as it does for PLAY TONE.
int fillBBCBuffer(char *buf, int bufsize)
{
	int16_t *samples = (int16_t *)buf;
	int frames = bufsize / (2 * (int)sizeof(int16_t));
	int n = 0, s, i;
	struct bbcstate *b = bbc;

	if (b == NULL)
	{ /* cannot happen while P_BBC, but never dereference NULL from the idle loop */
		memset(buf, 0, frames * 2 * sizeof(int16_t));
		playreadcomplete = 1;
		return frames * 2;
	}

	for (s = 0; s < frames; s++)
	{
		int32_t mix = 0;

		b->cs_acc += 100;
		if (b->cs_acc >= BBC_RATE)
		{
			b->cs_acc -= BBC_RATE;
			tick_100hz();
		}

		for (i = 1; i < 4; i++)
		{
			struct bbcchan *c = &b->ch[i];
			if (c->active && c->level)
			{
				uint32_t ph, nc, u, t;
				int32_t v;

				c->phase += c->inc;
				ph = c->phase;
				nc = c->inc;
				v = (ph & 0x80000000u) ? c->level : -c->level;

				/* polyBLEP.  A square that can only flip on sample
				 * boundaries carries alias images that beat against
				 * the true harmonics - a pitch-dependent shimmer on
				 * sustained notes, worst at the top of the range.
				 * The band-limited step differs from the naive one
				 * only within a sample of each edge, and there the
				 * residual is (1-tau)^2 of the step toward the
				 * transition midpoint - which for a square centred
				 * on zero is just a scale-down of the sample's own
				 * value.  tau in Q8; only edge-adjacent samples (a
				 * few hundred per second per voice) reach the divide.
				 * Edges: wrap = fall, half = rise. */
				if (ph < nc)
					u = ph; /* just after fall */
				else if (ph > (uint32_t)-nc)
					u = (uint32_t)-ph; /* just before fall */
				else if ((ph - 0x80000000u) < nc)
					u = ph - 0x80000000u; /* just after rise */
				else if ((0x80000000u - ph) < nc)
					u = 0x80000000u - ph; /* just before rise */
				else
					u = ~0u;
				if (u != ~0u && (t = u / (nc >> 8)) < 256)
				{
					t = 256 - t;
					v -= (int32_t)(v * (int32_t)(t * t)) >> 16;
				}
				mix += v;
			}
		}
		/* channel 0: noise.  The pitch is three bits, as the BBC's is - bits
		 * 0-1 choose the rate and bit 2 the kind.  0-2 and 4-6 are the three
		 * fixed rates, 0 the highest (4 << n at 44100 is the same rate as the
		 * original's 2 << n at 22050); 3 and 7 take their rate from channel 1
		 * instead, which is how a BBC game tunes a noise - Elite's explosion
		 * sweeps channel 1 down and the noise falls with it.  Below 4 the
		 * feedback is a single tap, so one bit cycles round the fifteen stages
		 * and the result is the pitched buzz the User Guide calls a sawtooth
		 * tone; at 4 and above two taps are XORed and it is a hiss.
		 *
		 * Channel 1's increment is read rather than its phase, so a channel 1
		 * note with an amplitude of 0 still tunes the noise, as writing the
		 * tone register does on the real chip, and a pitch envelope on
		 * channel 1 bends the noise with it.  It is doubled because a square
		 * toggles twice a cycle and the register is clocked by the toggles.
		 * Until channel 1 has a pitch there is nothing to be tuned by, and
		 * those two settings stay silent rather than sit at a DC level. */
		if (b->ch[0].active && b->ch[0].level)
		{
			int np = b->ch[0].pitch & 7;
			int tuned = ((np & 3) == 3);
			if (!tuned || b->ch[1].inc)
			{
				int advance;
				if (tuned)
				{
					uint32_t prev = b->noise_phase;
					b->noise_phase += b->ch[1].inc << 1;
					advance = (b->noise_phase < prev);
				}
				else
				{
					advance = (++b->noise_ctr >= (4u << (np & 3)));
					if (advance)
						b->noise_ctr = 0;
				}
				if (advance)
				{
					/* fifteen stages: two taps XORed for white, one for periodic */
					uint32_t bit = (np & 4)
									   ? ((b->noise_lfsr ^ (b->noise_lfsr >> 1)) & 1)
									   : (b->noise_lfsr & 1);
					b->noise_lfsr = (b->noise_lfsr >> 1) | (bit << 14);
				}
				mix += (b->noise_lfsr & 1) ? b->ch[0].level : -b->ch[0].level;
			}
		}

		mix *= 64; /* 4 x 126 x 64 = 32256 max */
		if (mix > 32767)
			mix = 32767;
		if (mix < -32768)
			mix = -32768;
		samples[n++] = (int16_t)mix;
		samples[n++] = (int16_t)mix;
	}

	/* Nothing playing and nothing waiting: let the output go.  The block
	 * just written is silence, so the previous one's tail plays out. */
	for (i = 0; i < 4; i++)
		if (b->ch[i].active || b->ch[i].qr != b->ch[i].qw)
			return n;
	playreadcomplete = 1;
	return n;
}

/* --- the commands' side ----------------------------------------------------- */

// Queue one note.  chan is the BBC channel word: bits 0-1 the channel,
// &10 flush that channel's queue and stop its note first, &100-&300 the
// sync group size.  amp is -15..0 for a volume or 1..16 for an envelope.
// Returns 0, or -1 if the channel's queue is full (a flush never fails).
int BBCSoundQueue(int chan, int amp, int pitch, int dur)
{
	int cn = chan & 3;
	struct bbcchan *c = &bbc_alloc()->ch[cn];
	struct bbcnote n;

	if (chan & 0x10)
	{ /* flush */
		c->qr = c->qw;
		c->active = 0;
	}

	if (amp > 0)
		n.amp = 0x80 | (amp > 16 ? 16 : amp);
	else
		n.amp = (-amp) > 15 ? 15 : -amp;
	n.pitch = pitch & 0xFF;
	n.dur = dur > 255 ? 255 : dur;
	n.sync = (chan >> 8) & 3;

	if ((uint8_t)(c->qw - c->qr) >= BBC_QLEN)
		return -1; /* queue full */
	c->q[c->qw % BBC_QLEN] = n;
	c->qw++;
	try_dequeue();
	return 0;
}

// Free slots in a channel's note queue - how many more notes PLAY BBC SOUND
// will take for it without blocking.  chan is 0-3, or -1 for the emptiest of
// the four.  The note that is playing has already left the queue (try_dequeue
// advances qr when it starts it), so this counts pending notes only.  A query
// before the first PLAY BBC must not take the heap block just to answer, so a
// NULL engine reports the full depth, which is what its empty queues hold.
int BBCSoundFree(int chan)
{
	int i, least = BBC_QLEN;

	if (bbc == NULL)
		return BBC_QLEN;
	if (chan >= 0)
	{
		struct bbcchan *c = &bbc->ch[chan & 3];
		return BBC_QLEN - (uint8_t)(c->qw - c->qr);
	}
	for (i = 0; i < 4; i++)
	{
		int f = BBC_QLEN - (uint8_t)(bbc->ch[i].qw - bbc->ch[i].qr);
		if (f < least)
			least = f;
	}
	return least;
}

// e[0] = envelope number 1-16, e[1..13] = T, PI1-3, PN1-3, AA, AD, AS, AR, ALA, ALD
void BBCEnvelope(const uint8_t *e)
{
	int n = e[0];
	if (n < 1 || n > 16)
		return;
	memcpy(bbc_alloc()->envs[n], e + 1, 13);
}

// Silence every channel and empty every queue.  Envelopes are kept.
void BBCSoundReset(void)
{
	int i;
	if (bbc == NULL)
		return;
	for (i = 0; i < 4; i++)
	{
		bbc->ch[i].qr = bbc->ch[i].qw;
		bbc->ch[i].active = 0;
		bbc->ch[i].level = 0;
	}
	bbc->cs_acc = 0;
}

// Give the state block back to the heap: ClearRuntime's call, before the
// heap itself is cleared.  Envelopes go with it.
void BBCSoundRelease(void)
{
	FreeMemorySafe((void **)&bbc);
}
/*  @endcond */
