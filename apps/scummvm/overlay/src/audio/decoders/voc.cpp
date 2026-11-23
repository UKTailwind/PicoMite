/* Cabal - Legacy Game Implementations
 *
 * Cabal is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

// Based on the ScummVM (GPLv2+) file of the same name

// PicoMite (apps/scummvm/overlay): the sound is mixed in the loader's sound fill, an interrupt, which may call no
// service, so a VOC stream may not read the SD card as it plays. VocStream reads its samples into memory when it is
// made (in the program's own code: a line of speech from MONSTER.SOU is a few hundred KB at most), and plays them from
// there. A stream too large for that, or with no memory for it, plays nothing.

#include "common/debug.h"
#include "common/endian.h"
#include "common/util.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/list.h"
#include "common/memstream.h"

#include "audio/audiostream.h"
#include "audio/decoders/pcm.h"
#include "audio/decoders/voc.h"

namespace Audio {

namespace {

bool checkVOCHeader(Common::ReadStream &stream) {
	VocFileHeader fileHeader;

	if (stream.read(&fileHeader, 8) != 8)
		return false;

	if (!memcmp(&fileHeader, "VTLK", 4)) {
		if (stream.read(&fileHeader, sizeof(VocFileHeader)) != sizeof(VocFileHeader))
			return false;
	} else if (!memcmp(&fileHeader, "Creative", 8)) {
		if (stream.read(((byte *)&fileHeader) + 8, sizeof(VocFileHeader) - 8) != sizeof(VocFileHeader) - 8)
			return false;
	} else {
		return false;
	}

	if (memcmp(fileHeader.desc, "Creative Voice File", 19) != 0)
		return false;
	//if (fileHeader.desc[19] != 0x1A)
	//	debug(3, "checkVOCHeader: Partially invalid header");

	int32 offset = FROM_LE_16(fileHeader.datablock_offset);
	int16 version = FROM_LE_16(fileHeader.version);
	int16 code = FROM_LE_16(fileHeader.id);

	if (offset != sizeof(VocFileHeader))
		return false;

	// 0x100 is an invalid VOC version used by German version of DOTT (Disk) and
	// French version of Simon the Sorcerer 2 (CD)
	if (version != 0x010A && version != 0x0114 && version != 0x0100)
		return false;

	if (code != ~version + 0x1234)
		return false;

	return true;
}

class VocStream : public SeekableAudioStream {
public:
	VocStream(Common::SeekableReadStream *stream, bool isUnsigned, DisposeAfterUse::Flag disposeAfterUse);
	~VocStream();

	virtual int readBuffer(int16 *buffer, const int numSamples);

	virtual uint getChannels() const { return 1; }

	virtual int getRate() const { return _rate; }

	virtual bool endOfData() const { return (_curBlock == _blocks.end()) && (_blockLeft == 0); }

	virtual bool seek(const Common::Timestamp &where);

	virtual Common::Timestamp getLength() const { return _length; }
private:
	void preProcess();
	void inMemory(int32 start);                  // PicoMite: the samples read into memory (above)

	Common::SeekableReadStream *_stream;         // PicoMite: no longer const, as inMemory replaces it
	DisposeAfterUse::Flag _disposeAfterUse;
	uint32 _end;                                 // PicoMite: where the last block ends, as preProcess found it

	const bool _isUnsigned;

	int _rate;
	Common::Timestamp _length;

	struct Block {
		uint8 code;
		uint32 length;

		union {
			struct {
				uint32 offset;
				int rate;
				int samples;
			} sampleBlock;

			struct {
				int count;
			} loopBlock;
		};
	};

	typedef Common::List<Block> BlockList;
	BlockList _blocks;

	BlockList::const_iterator _curBlock;
	uint32 _blockLeft;

	/**
	 * Advance one block in the stream in case
	 * the current one is empty.
	 */
	void updateBlockIfNeeded();

	// Do some internal buffering for systems with really slow slow disk i/o
	enum {
		/**
		 * How many samples we can buffer at once.
		 *
		 * TODO: Check whether this size suffices
		 * for systems with slow disk I/O.
		 */
		kSampleBufferLength = 2048
	};
	byte _buffer[kSampleBufferLength];

	/**
	 * Fill the temporary sample buffer used in readBuffer.
	 *
	 * @param maxSamples Maximum samples to read.
	 * @return actual count of samples read.
	 */
	int fillBuffer(int maxSamples);
};

VocStream::VocStream(Common::SeekableReadStream *stream, bool isUnsigned, DisposeAfterUse::Flag disposeAfterUse)
	: _stream(stream), _disposeAfterUse(disposeAfterUse), _isUnsigned(isUnsigned), _rate(0),
	  _length(), _blocks(), _curBlock(_blocks.end()), _blockLeft(0), _buffer(), _end(0) {
	const int32 start = _stream->pos();
	preProcess();
	inMemory(start);
}

// PicoMite: the stream from start to the end of its last block read into memory, and the blocks' offsets made relative
// to it. With no room for it, or a read that fails, the stream is left empty, so that nothing reads the card later.
void VocStream::inMemory(int32 start) {
	enum { kMaxInMemory = 4 * 1024 * 1024 };
	const uint32 len = (start >= 0 && _end > (uint32)start) ? _end - (uint32)start : 0;
	byte *buf = (len && len <= kMaxInMemory) ? (byte *)malloc(len) : 0;
	bool ok = buf != 0;
	if (ok) {
		_stream->seek(start, SEEK_SET);
		ok = !_stream->err() && _stream->read(buf, len) == len;
	}
	if (_disposeAfterUse == DisposeAfterUse::YES)
		delete _stream;
	if (!ok) {
		warning("VocStream: %u bytes of sound could not be read into memory", len);
		free(buf);
		buf = (byte *)malloc(1);                 // an empty stream, so that the class's pointer is never NULL
		_blocks.clear();
		if (!_rate)
			_rate = 11025;                       // (a rate of 0 would divide by 0 in a Timestamp)
		_length = Common::Timestamp(0, _rate);
	}
	_stream = new Common::MemoryReadStream(buf, ok ? len : 0, DisposeAfterUse::YES);
	_disposeAfterUse = DisposeAfterUse::YES;
	_curBlock = _blocks.end();
	_blockLeft = 0;
	if (ok) {
		for (BlockList::iterator i = _blocks.begin(); i != _blocks.end(); ++i)
			if (i->code == 1 || i->code == 9)
				i->sampleBlock.offset -= start;
		rewind();
	}
}

VocStream::~VocStream() {
	if (_disposeAfterUse == DisposeAfterUse::YES)
		delete _stream;
}

int VocStream::readBuffer(int16 *buffer, const int numSamples) {
	int samplesLeft = numSamples;
	while (samplesLeft > 0) {
		// Try to read up to "samplesLeft" samples.
		int len = fillBuffer(samplesLeft);

		// In case we were not able to read any samples
		// we will stop reading here.
		if (!len)
			break;

		// Adjust the samples left to read.
		samplesLeft -= len;

		// Copy the data to the caller's buffer.
		const byte *src = _buffer;
		while (len-- > 0)
			*buffer++ = (*src++ << 8) ^ (_isUnsigned ? 0x8000 : 0);
	}

	return numSamples - samplesLeft;
}

void VocStream::updateBlockIfNeeded() {
	// Have we now finished this block? If so, read the next block
	if (_blockLeft == 0 && _curBlock != _blocks.end()) {
		// Find the next sample block
		while (true) {
			// Next block
			++_curBlock;

			// Check whether we reached the end of the stream
			// yet.
			if (_curBlock == _blocks.end())
				return;

			// Skip all none sample blocks for now
			if (_curBlock->code != 1 && _curBlock->code != 9)
				continue;

			_stream->seek(_curBlock->sampleBlock.offset, SEEK_SET);

			// In case of an error we will stop
			// stream playback.
			if (_stream->err()) {
				_blockLeft = 0;
				_curBlock = _blocks.end();
			} else {
				_blockLeft = _curBlock->sampleBlock.samples;
			}

			return;
		}
	}
}

int VocStream::fillBuffer(int maxSamples) {
	int bufferedSamples = 0;
	byte *dst = _buffer;

	// We can only read up to "kSampleBufferLength" samples
	// so we take this into consideration, when trying to
	// read up to maxSamples.
	maxSamples = MIN<int>(kSampleBufferLength, maxSamples);

	// We will only read up to maxSamples
	while (maxSamples > 0 && !endOfData()) {
		// Calculate how many samples we can safely read
		// from the current block.
		const int len = MIN<int>(maxSamples, _blockLeft);

		// Try to read all the sample data and update the
		// destination pointer.
		const int bytesRead = _stream->read(dst, len);
		dst += bytesRead;

		// Calculate how many samples we actually read.
		const int samplesRead = bytesRead;

		// Update all status variables
		bufferedSamples += samplesRead;
		maxSamples -= samplesRead;
		_blockLeft -= samplesRead;

		// In case of an error we will stop
		// stream playback.
		if (_stream->err()) {
			_blockLeft = 0;
			_curBlock = _blocks.end();
			break;
		}

		// Advance to the next block in case the current
		// one is already finished.
		updateBlockIfNeeded();
	}

	return bufferedSamples;
}

bool VocStream::seek(const Common::Timestamp &where) {
	// Invalidate stream
	_blockLeft = 0;
	_curBlock = _blocks.end();

	if (where > _length)
		return false;

	// Search for the block containing the requested sample
	const uint32 seekSample = convertTimeToStreamPos(where, getRate(), getChannels()).totalNumberOfFrames();
	uint32 curSample = 0;

	for (_curBlock = _blocks.begin(); _curBlock != _blocks.end(); ++_curBlock) {
		// Skip all none sample blocks for now
		if (_curBlock->code != 1 && _curBlock->code != 9)
			continue;

		uint32 nextBlockSample = curSample + _curBlock->sampleBlock.samples;

		if (nextBlockSample > seekSample)
			break;

		curSample = nextBlockSample;
	}

	if (_curBlock == _blocks.end()) {
		return ((seekSample - curSample) == 0);
	} else {
		const uint32 offset = seekSample - curSample;

		_stream->seek(_curBlock->sampleBlock.offset + offset, SEEK_SET);

		// In case of an error we will stop
		// stream playback.
		if (_stream->err()) {
			_blockLeft = 0;
			_curBlock = _blocks.end();
		} else {
			_blockLeft = _curBlock->sampleBlock.samples - offset;
		}

		return true;
	}
}

void VocStream::preProcess() {
	Block block;

	// Scan through the file and collect all blocks
	while (true) {
		block.code = _stream->readByte();
		block.length = 0;

		// If we hit EOS here we found the end of the VOC file.
		// According to http://wiki.multimedia.cx/index.php?title=Creative_Voice
		// there is no need for an "Terminator" block to be present.
		// In case we hit a "Terminator" block we also break here.
		if (_stream->eos() || block.code == 0)
			break;
		// We will allow invalid block numbers as terminators. This is needed,
		// since some games ship broken VOC files. The following occasions are
		// known:
		// - 128 is used as terminator in Simon 1 Amiga CD32
		// - Full Throttle contains a VOC file with an incorrect block length
		//   resulting in a sample (127) to be read as block code.
		if (block.code > 9) {
			warning("VocStream::preProcess: Caught %d as terminator", block.code);
			break;
		}

		block.length = _stream->readByte();
		block.length |= _stream->readByte() << 8;
		block.length |= _stream->readByte() << 16;

		// Premature end of stream => error!
		if (_stream->eos() || _stream->err()) {
			warning("VocStream::preProcess: Reading failed");
			return;
		}

		uint32 skip = 0;

		switch (block.code) {
		// Sound data
		case 1:
		// Sound data (New format)
		case 9:
			if (block.code == 1) {
				if (block.length < 2) {
					warning("Invalid sound data block length %d in VOC file", block.length);
					return;
				}

				// Read header data
				int freqDiv = _stream->readByte();
				// Prevent division through 0
				if (freqDiv == 256) {
					warning("Invalid frequency divisor 256 in VOC file");
					return;
				}
				block.sampleBlock.rate = getSampleRateFromVOCRate(freqDiv);

				int codec = _stream->readByte();
				// We only support 8bit PCM
				if (codec != 0) {
					warning("Unhandled codec %d in VOC file", codec);
					return;
				}

				block.sampleBlock.samples = skip = block.length - 2;
				block.sampleBlock.offset = _stream->pos();

				// Check the last block if there is any
				if (_blocks.size() > 0) {
					BlockList::iterator lastBlock = _blocks.end();
					--lastBlock;
					// When we have found a block 8 as predecessor
					// we need to use its settings
					if (lastBlock->code == 8) {
						block.sampleBlock.rate = lastBlock->sampleBlock.rate;
						// Remove the block since we don't need it anymore
						_blocks.erase(lastBlock);
					}
				}
			} else {
				if (block.length < 12) {
					warning("Invalid sound data (wew format) block length %d in VOC file", block.length);
					return;
				}

				block.sampleBlock.rate = _stream->readUint32LE();
				int bitsPerSample = _stream->readByte();
				// We only support 8bit PCM
				if (bitsPerSample != 8) {
					warning("Unhandled bits per sample %d in VOC file", bitsPerSample);
					return;
				}
				int channels = _stream->readByte();
				// We only support mono
				if (channels != 1) {
					warning("Unhandled channel count %d in VOC file", channels);
					return;
				}
				int codec = _stream->readUint16LE();
				// We only support 8bit PCM
				if (codec != 0) {
					warning("Unhandled codec %d in VOC file", codec);
					return;
				}
				/*uint32 reserved = */_stream->readUint32LE();
				block.sampleBlock.offset = _stream->pos();
				block.sampleBlock.samples = skip = block.length - 12;
			}

			// Check whether we found a new highest rate
			if (_rate < block.sampleBlock.rate)
				_rate = block.sampleBlock.rate;
			break;

		// Silence
		case 3: {
			if (block.length != 3) {
				warning("Invalid silence block length %d in VOC file", block.length);
				return;
			}

			block.sampleBlock.offset = 0;

			block.sampleBlock.samples = _stream->readUint16LE() + 1;
			int freqDiv = _stream->readByte();
			// Prevent division through 0
			if (freqDiv == 256) {
				warning("Invalid frequency divisor 256 in VOC file");
				return;
			}
			block.sampleBlock.rate = getSampleRateFromVOCRate(freqDiv);
			} break;

		// Repeat start
		case 6:
			if (block.length != 2) {
				warning("Invalid repeat start block length %d in VOC file", block.length);
				return;
			}

			block.loopBlock.count = _stream->readUint16LE() + 1;
			break;

		// Repeat end
		case 7:
			break;

		// Extra info
		case 8: {
			if (block.length != 4)
				return;

			int freqDiv = _stream->readUint16LE();
			// Prevent division through 0
			if (freqDiv == 65536) {
				warning("Invalid frequency divisor 65536 in VOC file");
				return;
			}

			int codec = _stream->readByte();
			// We only support 8bit PCM.
			if (codec != 0) {
				warning("Unhandled codec %d in VOC file", codec);
				return;
			}

			int channels = _stream->readByte() + 1;
			// We only support mono sound right now
			if (channels != 1) {
				warning("Unhandled channel count %d in VOC file", channels);
				return;
			}

			block.sampleBlock.offset = 0;
			block.sampleBlock.samples = 0;
			block.sampleBlock.rate = 256000000L / (65536L - freqDiv);
			} break;

		default:
			warning("Unhandled code %d in VOC file (len %d)", block.code, block.length);
			// Skip the whole block and try to use the next one.
			skip = block.length;
		}

		// Premature end of stream => error!
		if (_stream->eos() || _stream->err()) {
			warning("VocStream::preProcess: Reading failed");
			return;
		}

		// Skip the rest of the block
		if (skip)
			_stream->skip(skip);

		_blocks.push_back(block);
	}
	_end = _stream->pos();                       // PicoMite: (inMemory)

	// Since we determined the sample rate we need for playback now, we will
	// initialize the play length.
	_length = Common::Timestamp(0, _rate);

	// Calculate the total play time and do some more sanity checks
	for (BlockList::const_iterator i = _blocks.begin(), end = _blocks.end(); i != end; ++i) {
		// Check whether we found a block 8 which survived, this is not
		// allowed to happen!
		if (i->code == 8) {
			warning("VOC file contains unused block 8");
			return;
		}

		// For now only use blocks with actual samples
		if (i->code != 1 && i->code != 9)
			continue;

		// Check the sample rate
		if (i->sampleBlock.rate != _rate) {
			warning("VOC file contains chunks with different sample rates (%d != %d)", _rate, i->sampleBlock.rate);
			return;
		}

		_length = _length.addFrames(i->sampleBlock.samples);
	}

	// Set the current block to the first block in the stream
	rewind();
}

} // End of anonymous namespace

int getSampleRateFromVOCRate(int vocSR) {
	if (vocSR == 0xa5 || vocSR == 0xa6) {
		return 11025;
	} else if (vocSR == 0xd2 || vocSR == 0xd3) {
		return 22050;
	} else {
		int sr = 1000000L / (256L - vocSR);
		// inexact sampling rates occur e.g. in the kitchen in Monkey Island,
		// very easy to reach right from the start of the game.
		//warning("inexact sample rate used: %i (0x%x)", sr, vocSR);
		return sr;
	}
}

SeekableAudioStream *makeVOCStream(Common::SeekableReadStream *stream, byte flags, DisposeAfterUse::Flag disposeAfterUse) {
	if (!checkVOCHeader(*stream)) {
		if (disposeAfterUse == DisposeAfterUse::YES)
			delete stream;
		return 0;
	}

	SeekableAudioStream *audioStream = new VocStream(stream, (flags & Audio::FLAG_UNSIGNED) != 0, disposeAfterUse);

	if (audioStream->endOfData()) {
		delete audioStream;
		return 0;
	} else {
		return audioStream;
	}
}

} // End of namespace Audio
