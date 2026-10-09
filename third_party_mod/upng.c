/*
uPNG -- derived from LodePNG version 20100808
Copyright (c) 2005-2010 Lode Vandevenne
Copyright (c) 2010 Sean Middleditch
This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:
		1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgment in the product documentation would be
		appreciated but is not required.
		2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.
		3. This notice may not be removed or altered from any source
		distribution.
*/


/*
 * PicoMite: the decoder streams.  upng_new_from_file opens the file,
 * upng_header reads the chunks up to the first IDAT (the header and the
 * palette), and upng_decode_rows inflates the IDAT data as it is read from the
 * file, through a window of at least 32 KB, unfilters each scanline as it
 * completes and hands it to a callback as RGBA8888.  Nothing the size of the
 * file or the image is held: on a large PNG those buffers (zeroed by
 * GetMemory) and the copies between them starved the audio for hundreds of
 * milliseconds, and they needed megabytes of PSRAM.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

#define MAKE_BYTE(b) ((b) & 0xFF)
#define MAKE_DWORD(a, b, c, d) ((MAKE_BYTE(a) << 24) | (MAKE_BYTE(b) << 16) | (MAKE_BYTE(c) << 8) | MAKE_BYTE(d))
#define MAKE_DWORD_PTR(p) MAKE_DWORD((p)[0], (p)[1], (p)[2], (p)[3])

#define CHUNK_IHDR MAKE_DWORD('I', 'H', 'D', 'R')
#define CHUNK_IDAT MAKE_DWORD('I', 'D', 'A', 'T')
#define CHUNK_IEND MAKE_DWORD('I', 'E', 'N', 'D')
#define CHUNK_PLTE MAKE_DWORD('P', 'L', 'T', 'E')
#define CHUNK_TRNS MAKE_DWORD('t', 'R', 'N', 'S')

#define FIRST_LENGTH_CODE_INDEX 257
#define LAST_LENGTH_CODE_INDEX 285

#define NUM_DEFLATE_CODE_SYMBOLS 288 /*256 literals, the end code, some length codes, and 2 unused codes */
#define NUM_DISTANCE_SYMBOLS 32		 /*the distance codes have their own symbols, 30 used, 2 unused */
#define NUM_CODE_LENGTH_CODES 19	 /*the code length codes. 0-15: code lengths, 16: copy previous 3-6 times, 17: 3-10 zeros, 18: 11-138 zeros */
#define MAX_SYMBOLS 288				 /* largest number of symbols used by any tree type */

#define DEFLATE_CODE_BITLEN 15
#define DISTANCE_BITLEN 15
#define CODE_LENGTH_BITLEN 7
#define MAX_BIT_LENGTH 15 /* largest bitlen used by any tree type */

#define DEFLATE_CODE_BUFFER_SIZE (NUM_DEFLATE_CODE_SYMBOLS * 2)
#define DISTANCE_BUFFER_SIZE (NUM_DISTANCE_SYMBOLS * 2)
#define CODE_LENGTH_BUFFER_SIZE (NUM_DISTANCE_SYMBOLS * 2)

#define SET_ERROR(upng, code)          \
	do                                 \
	{                                  \
		(upng)->error = (code);        \
		(upng)->error_line = __LINE__; \
	} while (0)

#define upng_chunk_length(chunk) MAKE_DWORD_PTR(chunk)
#define upng_chunk_type(chunk) MAKE_DWORD_PTR((chunk) + 4)
#define upng_chunk_critical(chunk) (((chunk)[4] & 32) == 0)
typedef enum upng_state
{
	UPNG_ERROR = -1,
	UPNG_DECODED = 0,
	UPNG_HEADER = 1,
	UPNG_NEW = 2
} upng_state;

typedef enum upng_color
{
	UPNG_LUM = 0,
	UPNG_RGB = 2,
	UPNG_PLT = 3,
	UPNG_LUMA = 4,
	UPNG_RGBA = 6
} upng_color;

#define PNG_INBUF 512

struct upng_t
{
	unsigned width;
	unsigned height;

	upng_color color_type;
	unsigned color_depth;
	upng_format format;

	unsigned char *buffer; /* the window and row buffers while decoding */

	upng_error error;
	unsigned error_line;

	upng_state state;

	/* the file, and the zlib stream through its IDAT chunks */
	int fnbr;
	unsigned long chunk_left; /* bytes of the current IDAT chunk not yet read */
	unsigned in_pos, in_len;  /* the bytes of inbuf not yet used */
	unsigned bitbuf, bitcnt;  /* bits read from the stream, least significant first */
	unsigned char inbuf[PNG_INBUF];

	/* palette for indexed (color_type 3) PNGs */
	unsigned char palette[256][4]; /* R, G, B, A per entry */
	unsigned palette_entries;
	unsigned trns_entries;
};

/* the inflated data: a window of the most recent bytes, at least the 32 KB a
   back-reference can reach, and the scanline being completed in it */
typedef struct png_out
{
	unsigned char *win;
	unsigned long mask;		/* window size - 1: a power of two */
	unsigned long pos;		/* bytes inflated so far */
	unsigned long rowstart; /* where the scanline being completed starts */
	unsigned long rowlen;	/* its filter byte and linebytes */
	unsigned linebytes, bytewidth;
	unsigned char *cur, *prev, *rgba; /* this and the last unfiltered scanline, the RGBA expansion */
	unsigned y;
	int done; /* every row delivered, or the callback has had enough */
	upng_row_fn row;
	void *ctx;
} png_out;

typedef struct huffman_tree
{
	unsigned *tree2d;
	unsigned maxbitlen; /*maximum number of bits a single code can get */
	unsigned numcodes;	/*number of symbols in the alphabet = number of codes */
} huffman_tree;

static const unsigned LENGTH_BASE[29] = {/*the base lengths represented by codes 257-285 */
										 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
										 67, 83, 99, 115, 131, 163, 195, 227, 258};

static const unsigned LENGTH_EXTRA[29] = {/*the extra bits used by codes 257-285 (added to base length) */
										  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5,
										  5, 5, 5, 0};

static const unsigned DISTANCE_BASE[30] = {/*the base backwards distances (the bits of distance codes appear after length codes and use their own huffman tree) */
										   1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
										   769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

static const unsigned DISTANCE_EXTRA[30] = {/*the extra bits of backwards distances (added to base) */
											0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
											11, 11, 12, 12, 13, 13};

static const unsigned CLCL[NUM_CODE_LENGTH_CODES] /*the order in which "code length alphabet code lengths" are stored, out of this the huffman tree of the dynamic huffman tree lengths is generated */
	= {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

static const unsigned FIXED_DEFLATE_CODE_TREE[NUM_DEFLATE_CODE_SYMBOLS * 2] = {
	289, 370, 290, 307, 546, 291, 561, 292, 293, 300, 294, 297, 295, 296, 0, 1,
	2, 3, 298, 299, 4, 5, 6, 7, 301, 304, 302, 303, 8, 9, 10, 11, 305, 306, 12,
	13, 14, 15, 308, 339, 309, 324, 310, 317, 311, 314, 312, 313, 16, 17, 18,
	19, 315, 316, 20, 21, 22, 23, 318, 321, 319, 320, 24, 25, 26, 27, 322, 323,
	28, 29, 30, 31, 325, 332, 326, 329, 327, 328, 32, 33, 34, 35, 330, 331, 36,
	37, 38, 39, 333, 336, 334, 335, 40, 41, 42, 43, 337, 338, 44, 45, 46, 47,
	340, 355, 341, 348, 342, 345, 343, 344, 48, 49, 50, 51, 346, 347, 52, 53,
	54, 55, 349, 352, 350, 351, 56, 57, 58, 59, 353, 354, 60, 61, 62, 63, 356,
	363, 357, 360, 358, 359, 64, 65, 66, 67, 361, 362, 68, 69, 70, 71, 364,
	367, 365, 366, 72, 73, 74, 75, 368, 369, 76, 77, 78, 79, 371, 434, 372,
	403, 373, 388, 374, 381, 375, 378, 376, 377, 80, 81, 82, 83, 379, 380, 84,
	85, 86, 87, 382, 385, 383, 384, 88, 89, 90, 91, 386, 387, 92, 93, 94, 95,
	389, 396, 390, 393, 391, 392, 96, 97, 98, 99, 394, 395, 100, 101, 102, 103,
	397, 400, 398, 399, 104, 105, 106, 107, 401, 402, 108, 109, 110, 111, 404,
	419, 405, 412, 406, 409, 407, 408, 112, 113, 114, 115, 410, 411, 116, 117,
	118, 119, 413, 416, 414, 415, 120, 121, 122, 123, 417, 418, 124, 125, 126,
	127, 420, 427, 421, 424, 422, 423, 128, 129, 130, 131, 425, 426, 132, 133,
	134, 135, 428, 431, 429, 430, 136, 137, 138, 139, 432, 433, 140, 141, 142,
	143, 435, 483, 436, 452, 568, 437, 438, 445, 439, 442, 440, 441, 144, 145,
	146, 147, 443, 444, 148, 149, 150, 151, 446, 449, 447, 448, 152, 153, 154,
	155, 450, 451, 156, 157, 158, 159, 453, 468, 454, 461, 455, 458, 456, 457,
	160, 161, 162, 163, 459, 460, 164, 165, 166, 167, 462, 465, 463, 464, 168,
	169, 170, 171, 466, 467, 172, 173, 174, 175, 469, 476, 470, 473, 471, 472,
	176, 177, 178, 179, 474, 475, 180, 181, 182, 183, 477, 480, 478, 479, 184,
	185, 186, 187, 481, 482, 188, 189, 190, 191, 484, 515, 485, 500, 486, 493,
	487, 490, 488, 489, 192, 193, 194, 195, 491, 492, 196, 197, 198, 199, 494,
	497, 495, 496, 200, 201, 202, 203, 498, 499, 204, 205, 206, 207, 501, 508,
	502, 505, 503, 504, 208, 209, 210, 211, 506, 507, 212, 213, 214, 215, 509,
	512, 510, 511, 216, 217, 218, 219, 513, 514, 220, 221, 222, 223, 516, 531,
	517, 524, 518, 521, 519, 520, 224, 225, 226, 227, 522, 523, 228, 229, 230,
	231, 525, 528, 526, 527, 232, 233, 234, 235, 529, 530, 236, 237, 238, 239,
	532, 539, 533, 536, 534, 535, 240, 241, 242, 243, 537, 538, 244, 245, 246,
	247, 540, 543, 541, 542, 248, 249, 250, 251, 544, 545, 252, 253, 254, 255,
	547, 554, 548, 551, 549, 550, 256, 257, 258, 259, 552, 553, 260, 261, 262,
	263, 555, 558, 556, 557, 264, 265, 266, 267, 559, 560, 268, 269, 270, 271,
	562, 565, 563, 564, 272, 273, 274, 275, 566, 567, 276, 277, 278, 279, 569,
	572, 570, 571, 280, 281, 282, 283, 573, 574, 284, 285, 286, 287, 0, 0};

static const unsigned FIXED_DISTANCE_TREE[NUM_DISTANCE_SYMBOLS * 2] = {
	33, 48, 34, 41, 35, 38, 36, 37, 0, 1, 2, 3, 39, 40, 4, 5, 6, 7, 42, 45, 43,
	44, 8, 9, 10, 11, 46, 47, 12, 13, 14, 15, 49, 56, 50, 53, 51, 52, 16, 17,
	18, 19, 54, 55, 20, 21, 22, 23, 57, 60, 58, 59, 24, 25, 26, 27, 61, 62, 28,
	29, 30, 31, 0, 0};


/* read exactly n bytes of the file */
static int png_read(upng_t *upng, void *buf, unsigned n)
{
	unsigned int got = 0;
	FileGetData(upng->fnbr, buf, n, &got);
	return got == n;
}

/* skip n bytes of the file (inbuf is free until the image data starts) */
static int png_skip(upng_t *upng, unsigned long n)
{
	while (n)
	{
		unsigned k = (n < PNG_INBUF) ? n : PNG_INBUF;
		if (!png_read(upng, upng->inbuf, k))
			return 0;
		n -= k;
	}
	return 1;
}

/* the next byte of the zlib stream, which runs on through consecutive IDAT
   chunks; the audio is fed at every refill */
static unsigned png_byte(upng_t *upng)
{
	if (upng->error != UPNG_EOK)
		return 0;
	if (upng->in_pos == upng->in_len)
	{
		unsigned n;
		while (upng->chunk_left == 0)
		{ /* this IDAT is used up: skip its CRC, and the next chunk must be another */
			unsigned char c[12];
			if (!png_read(upng, c, 12) || MAKE_DWORD_PTR(c + 8) != CHUNK_IDAT)
			{
				SET_ERROR(upng, UPNG_EMALFORMED);
				return 0;
			}
			upng->chunk_left = MAKE_DWORD_PTR(c + 4);
		}
		n = (upng->chunk_left < PNG_INBUF) ? upng->chunk_left : PNG_INBUF;
		if (!png_read(upng, upng->inbuf, n))
		{
			SET_ERROR(upng, UPNG_EMALFORMED);
			return 0;
		}
		upng->chunk_left -= n;
		upng->in_pos = 0;
		upng->in_len = n;
		CheckAudio();
	}
	return upng->inbuf[upng->in_pos++];
}

static inline unsigned read_bit(upng_t *upng)
{
	unsigned result;
	if (upng->bitcnt == 0)
	{
		upng->bitbuf = png_byte(upng);
		upng->bitcnt = 8;
	}
	result = upng->bitbuf & 1;
	upng->bitbuf >>= 1;
	upng->bitcnt--;
	return result;
}

/* nbits (at most 16) bits, the first read in bit 0 */
static unsigned read_bits(upng_t *upng, unsigned nbits)
{
	unsigned result;
	while (upng->bitcnt < nbits)
	{
		upng->bitbuf |= png_byte(upng) << upng->bitcnt;
		upng->bitcnt += 8;
	}
	result = upng->bitbuf & ((1u << nbits) - 1);
	upng->bitbuf >>= nbits;
	upng->bitcnt -= nbits;
	return result;
}

/* the buffer must be numcodes*2 in size! */
static void huffman_tree_init(huffman_tree *tree, unsigned *buffer, unsigned numcodes, unsigned maxbitlen)
{
	tree->tree2d = buffer;

	tree->numcodes = numcodes;
	tree->maxbitlen = maxbitlen;
}

/*given the code lengths (as stored in the PNG file), generate the tree as defined by Deflate. maxbitlen is the maximum bits that a code in the tree can have. return value is error.*/
static void huffman_tree_create_lengths(upng_t *upng, huffman_tree *tree, const unsigned *bitlen)
{
	unsigned tree1d[MAX_SYMBOLS];
	unsigned blcount[MAX_BIT_LENGTH + 1]; /* code lengths run 0 to 15 */
	unsigned nextcode[MAX_BIT_LENGTH + 1];
	unsigned bits, n, i;
	unsigned nodefilled = 0; /*up to which node it is filled */
	unsigned treepos = 0;	 /*position in the tree (1 of the numcodes columns) */

	/* initialize local vectors */
	memset(blcount, 0, sizeof(blcount));
	memset(nextcode, 0, sizeof(nextcode));

	/*step 1: count number of instances of each code length */
	for (bits = 0; bits < tree->numcodes; bits++)
	{
		blcount[bitlen[bits]]++;
	}

	/*step 2: generate the nextcode values */
	for (bits = 1; bits <= tree->maxbitlen; bits++)
	{
		nextcode[bits] = (nextcode[bits - 1] + blcount[bits - 1]) << 1;
	}

	/*step 3: generate all the codes */
	for (n = 0; n < tree->numcodes; n++)
	{
		if (bitlen[n] != 0)
		{
			tree1d[n] = nextcode[bitlen[n]]++;
		}
	}

	/*convert tree1d[] to tree2d[][]. In the 2D array, a value of 32767 means uninited, a value >= numcodes is an address to another bit, a value < numcodes is a code. The 2 rows are the 2 possible bit values (0 or 1), there are as many columns as codes - 1
	   a good huffmann tree has N * 2 - 1 nodes, of which N - 1 are internal nodes. Here, the internal nodes are stored (what their 0 and 1 option point to). There is only memory for such good tree currently, if there are more nodes (due to too long length codes), error 55 will happen */
	for (n = 0; n < tree->numcodes * 2; n++)
	{
		tree->tree2d[n] = 32767; /*32767 here means the tree2d isn't filled there yet */
	}

	for (n = 0; n < tree->numcodes; n++)
	{ /*the codes */
		for (i = 0; i < bitlen[n]; i++)
		{ /*the bits for this code */
			unsigned char bit = (unsigned char)((tree1d[n] >> (bitlen[n] - i - 1)) & 1);
			/* check if oversubscribed */
			if (treepos > tree->numcodes - 2)
			{
				SET_ERROR(upng, UPNG_EMALFORMED);
				return;
			}

			if (tree->tree2d[2 * treepos + bit] == 32767)
			{ /*not yet filled in */
				if (i + 1 == bitlen[n])
				{										 /*last bit */
					tree->tree2d[2 * treepos + bit] = n; /*put the current code in it */
					treepos = 0;
				}
				else
				{ /*put address of the next step in here, first that address has to be found of course (it's just nodefilled + 1)... */
					nodefilled++;
					tree->tree2d[2 * treepos + bit] = nodefilled + tree->numcodes; /*addresses encoded with numcodes added to it */
					treepos = nodefilled;
				}
			}
			else
			{
				treepos = tree->tree2d[2 * treepos + bit] - tree->numcodes;
			}
		}
	}

	for (n = 0; n < tree->numcodes * 2; n++)
	{
		if (tree->tree2d[n] == 32767)
		{
			tree->tree2d[n] = 0; /*remove possible remaining 32767's */
		}
	}
}


static unsigned huffman_decode_symbol(upng_t *upng, const huffman_tree *codetree)
{
	unsigned treepos = 0, ct;
	for (;;)
	{
		ct = codetree->tree2d[(treepos << 1) | read_bit(upng)];
		if (upng->error != UPNG_EOK)
		{
			return 0; /* the data ran out */
		}
		if (ct < codetree->numcodes)
		{
			return ct;
		}

		treepos = ct - codetree->numcodes;
		if (treepos >= codetree->numcodes)
		{
			SET_ERROR(upng, UPNG_EMALFORMED);
			return 0;
		}
	}
}

/* get the tree of a deflated block with dynamic tree, the tree itself is also Huffman compressed with a known tree*/
static void get_tree_inflate_dynamic(upng_t *upng, huffman_tree *codetree, huffman_tree *codetreeD, huffman_tree *codelengthcodetree)
{
	unsigned codelengthcode[NUM_CODE_LENGTH_CODES];
	unsigned bitlen[NUM_DEFLATE_CODE_SYMBOLS];
	unsigned bitlenD[NUM_DISTANCE_SYMBOLS];
	unsigned n, hlit, hdist, hclen, i;

	/* clear bitlen arrays */
	memset(bitlen, 0, sizeof(bitlen));
	memset(bitlenD, 0, sizeof(bitlenD));

	hlit = read_bits(upng, 5) + 257; /*number of literal/length codes + 257. Unlike the spec, the value 257 is added to it here already */
	hdist = read_bits(upng, 5) + 1;	 /*number of distance codes. Unlike the spec, the value 1 is added to it here already */
	hclen = read_bits(upng, 4) + 4;	 /*number of code length codes. Unlike the spec, the value 4 is added to it here already */

	for (i = 0; i < NUM_CODE_LENGTH_CODES; i++)
	{
		if (i < hclen)
		{
			codelengthcode[CLCL[i]] = read_bits(upng, 3);
		}
		else
		{
			codelengthcode[CLCL[i]] = 0; /*if not, it must stay 0 */
		}
	}

	huffman_tree_create_lengths(upng, codelengthcodetree, codelengthcode);

	/* bail now if we encountered an error earlier */
	if (upng->error != UPNG_EOK)
	{
		return;
	}

	/*now we can use this tree to read the lengths for the tree that this function will return */
	i = 0;
	while (i < hlit + hdist)
	{ /*i is the current symbol we're reading in the part that contains the code lengths of lit/len codes and dist codes */
		unsigned code = huffman_decode_symbol(upng, codelengthcodetree);
		unsigned replength, value;
		if (upng->error != UPNG_EOK)
		{
			break;
		}

		if (code <= 15)
		{ /*a length code */
			if (i < hlit)
			{
				bitlen[i] = code;
			}
			else
			{
				bitlenD[i - hlit] = code;
			}
			i++;
			continue;
		}
		if (code == 16)
		{ /*repeat previous 3-6 times */
			if (i == 0)
			{ /* there is no previous */
				SET_ERROR(upng, UPNG_EMALFORMED);
				break;
			}
			replength = 3 + read_bits(upng, 2);
			value = ((i - 1) < hlit) ? bitlen[i - 1] : bitlenD[i - hlit - 1];
		}
		else if (code == 17)
		{ /*repeat "0" 3-10 times */
			replength = 3 + read_bits(upng, 3);
			value = 0;
		}
		else if (code == 18)
		{ /*repeat "0" 11-138 times */
			replength = 11 + read_bits(upng, 7);
			value = 0;
		}
		else
		{
			/* somehow an unexisting code appeared. This can never happen. */
			SET_ERROR(upng, UPNG_EMALFORMED);
			break;
		}

		/*repeat this value in the next lengths */
		for (n = 0; n < replength; n++)
		{
			/* i is larger than the amount of codes */
			if (i >= hlit + hdist)
			{
				SET_ERROR(upng, UPNG_EMALFORMED);
				break;
			}
			if (i < hlit)
			{
				bitlen[i] = value;
			}
			else
			{
				bitlenD[i - hlit] = value;
			}
			i++;
		}
	}

	/*the length of the end code 256 must be larger than 0 */
	if (upng->error == UPNG_EOK && bitlen[256] == 0)
	{
		SET_ERROR(upng, UPNG_EMALFORMED);
	}

	/*now we've finally got hlit and hdist, so generate the code trees, and the function is done */
	if (upng->error == UPNG_EOK)
	{
		huffman_tree_create_lengths(upng, codetree, bitlen);
	}
	if (upng->error == UPNG_EOK)
	{
		huffman_tree_create_lengths(upng, codetreeD, bitlenD);
	}
}

/*Paeth predicter, used by PNG filter type 4*/
static int paeth_predictor(int a, int b, int c)
{
	int p = a + b - c;
	int pa = p > a ? p - a : a - p;
	int pb = p > b ? p - b : b - p;
	int pc = p > c ? p - c : c - p;

	if (pa <= pb && pa <= pc)
		return a;
	else if (pb <= pc)
		return b;
	else
		return c;
}

static void unfilter_scanline(upng_t *upng, unsigned char *recon, const unsigned char *scanline, const unsigned char *precon, unsigned long bytewidth, unsigned char filterType, unsigned long length)
{
	/*
	   For PNG filter method 0
	   unfilter a PNG image scanline by scanline. when the pixels are smaller than 1 byte, the filter works byte per byte (bytewidth = 1)
	   precon is the previous unfiltered scanline, recon the result, scanline the current one
	   the incoming scanlines do NOT include the filtertype byte, that one is given in the parameter filterType instead
	   recon and scanline MAY be the same memory address! precon must be disjoint.
	 */

	unsigned long i;
	switch (filterType)
	{
	case 0:
		for (i = 0; i < length; i++)
			recon[i] = scanline[i];
		break;
	case 1:
		for (i = 0; i < bytewidth; i++)
			recon[i] = scanline[i];
		for (i = bytewidth; i < length; i++)
			recon[i] = scanline[i] + recon[i - bytewidth];
		break;
	case 2:
		if (precon)
			for (i = 0; i < length; i++)
				recon[i] = scanline[i] + precon[i];
		else
			for (i = 0; i < length; i++)
				recon[i] = scanline[i];
		break;
	case 3:
		if (precon)
		{
			for (i = 0; i < bytewidth; i++)
				recon[i] = scanline[i] + precon[i] / 2;
			for (i = bytewidth; i < length; i++)
				recon[i] = scanline[i] + ((recon[i - bytewidth] + precon[i]) / 2);
		}
		else
		{
			for (i = 0; i < bytewidth; i++)
				recon[i] = scanline[i];
			for (i = bytewidth; i < length; i++)
				recon[i] = scanline[i] + recon[i - bytewidth] / 2;
		}
		break;
	case 4:
		if (precon)
		{
			for (i = 0; i < bytewidth; i++)
				recon[i] = (unsigned char)(scanline[i] + paeth_predictor(0, precon[i], 0));
			for (i = bytewidth; i < length; i++)
				recon[i] = (unsigned char)(scanline[i] + paeth_predictor(recon[i - bytewidth], precon[i], precon[i - bytewidth]));
		}
		else
		{
			for (i = 0; i < bytewidth; i++)
				recon[i] = scanline[i];
			for (i = bytewidth; i < length; i++)
				recon[i] = (unsigned char)(scanline[i] + paeth_predictor(recon[i - bytewidth], 0, 0));
		}
		break;
	default:
		SET_ERROR(upng, UPNG_EMALFORMED);
		break;
	}
}


/* a scanline is complete in the window: unfilter it against the one before,
   expand an indexed one through the palette and hand it over */
static void emit_row(upng_t *upng, png_out *out)
{
	unsigned long src = out->rowstart;
	unsigned char filter = out->win[src++ & out->mask];
	const unsigned char *pixels = out->cur;
	unsigned char *t;
	unsigned i;

	for (i = 0; i < out->linebytes; i++)
	{
		out->cur[i] = out->win[src++ & out->mask];
	}
	unfilter_scanline(upng, out->cur, out->cur, out->y ? out->prev : NULL, out->bytewidth, filter, out->linebytes);
	if (upng->error != UPNG_EOK)
	{
		return;
	}
	if (upng->color_type == UPNG_PLT)
	{ /* indices of 1, 2, 4 or 8 bits, packed from the most significant bit */
		unsigned depth = upng->color_depth, mask = (1u << depth) - 1;
		for (i = 0; i < upng->width; i++)
		{
			unsigned bit = i * depth;
			unsigned idx = (out->cur[bit >> 3] >> (8 - depth - (bit & 7))) & mask;
			if (idx >= upng->palette_entries)
			{
				idx = 0;
			}
			memcpy(out->rgba + i * 4, upng->palette[idx], 4);
		}
		pixels = out->rgba;
	}
	if (!out->row(out->ctx, out->y, pixels))
	{
		out->done = 1;
	}
	t = out->cur;
	out->cur = out->prev;
	out->prev = t;
	out->rowstart += out->rowlen;
	if (++out->y == upng->height)
	{
		out->done = 1;
	}
	CheckAudio();
}

/* hand over every scanline completed so far */
static inline void png_flush(upng_t *upng, png_out *out)
{
	while (!out->done && upng->error == UPNG_EOK && out->pos - out->rowstart >= out->rowlen)
	{
		emit_row(upng, out);
	}
}

/*inflate a block with dynamic of fixed Huffman tree*/
static void inflate_huffman(upng_t *upng, png_out *out, unsigned btype)
{
	unsigned codetree_buffer[DEFLATE_CODE_BUFFER_SIZE];
	unsigned codetreeD_buffer[DISTANCE_BUFFER_SIZE];

	huffman_tree codetree;
	huffman_tree codetreeD;

	if (btype == 1)
	{
		/* fixed trees */
		huffman_tree_init(&codetree, (unsigned *)FIXED_DEFLATE_CODE_TREE, NUM_DEFLATE_CODE_SYMBOLS, DEFLATE_CODE_BITLEN);
		huffman_tree_init(&codetreeD, (unsigned *)FIXED_DISTANCE_TREE, NUM_DISTANCE_SYMBOLS, DISTANCE_BITLEN);
	}
	else
	{
		/* dynamic trees */
		unsigned codelengthcodetree_buffer[CODE_LENGTH_BUFFER_SIZE];
		huffman_tree codelengthcodetree;

		huffman_tree_init(&codetree, codetree_buffer, NUM_DEFLATE_CODE_SYMBOLS, DEFLATE_CODE_BITLEN);
		huffman_tree_init(&codetreeD, codetreeD_buffer, NUM_DISTANCE_SYMBOLS, DISTANCE_BITLEN);
		huffman_tree_init(&codelengthcodetree, codelengthcodetree_buffer, NUM_CODE_LENGTH_CODES, CODE_LENGTH_BITLEN);
		get_tree_inflate_dynamic(upng, &codetree, &codetreeD, &codelengthcodetree);
	}

	while (!out->done && upng->error == UPNG_EOK)
	{
		unsigned code = huffman_decode_symbol(upng, &codetree);
		if (upng->error != UPNG_EOK || code == 256)
		{
			return; /* an error, or the end of the block */
		}

		if (code <= 255)
		{
			/* literal symbol */
			out->win[out->pos++ & out->mask] = (unsigned char)code;
		}
		else if (code <= LAST_LENGTH_CODE_INDEX)
		{
			/* a length, then a distance back into the window */
			unsigned long length = LENGTH_BASE[code - FIRST_LENGTH_CODE_INDEX];
			unsigned long distance;
			unsigned codeD;

			length += read_bits(upng, LENGTH_EXTRA[code - FIRST_LENGTH_CODE_INDEX]);
			codeD = huffman_decode_symbol(upng, &codetreeD);
			if (upng->error != UPNG_EOK)
			{
				return;
			}
			/* invalid distance code (30-31 are never used) */
			if (codeD > 29)
			{
				SET_ERROR(upng, UPNG_EMALFORMED);
				return;
			}
			distance = DISTANCE_BASE[codeD] + read_bits(upng, DISTANCE_EXTRA[codeD]);
			if (distance > out->pos)
			{ /* back before the start of the data */
				SET_ERROR(upng, UPNG_EMALFORMED);
				return;
			}
			while (length--)
			{
				out->win[out->pos & out->mask] = out->win[(out->pos - distance) & out->mask];
				out->pos++;
			}
		}
		else
		{
			SET_ERROR(upng, UPNG_EMALFORMED);
			return;
		}
		png_flush(upng, out);
	}
}

/* a block stored without compression */
static void inflate_stored(upng_t *upng, png_out *out)
{
	unsigned len, nlen;

	/* go to first boundary of byte */
	upng->bitbuf >>= upng->bitcnt & 7;
	upng->bitcnt -= upng->bitcnt & 7;
	len = read_bits(upng, 16);
	nlen = read_bits(upng, 16);

	/* check if 16-bit nlen is really the one's complement of len */
	if (len + nlen != 65535)
	{
		SET_ERROR(upng, UPNG_EMALFORMED);
		return;
	}
	while (len-- && !out->done && upng->error == UPNG_EOK)
	{
		out->win[out->pos++ & out->mask] = (unsigned char)read_bits(upng, 8);
		png_flush(upng, out);
	}
}

static upng_format determine_format(upng_t *upng)
{
	switch (upng->color_type)
	{
	case UPNG_LUM:
		switch (upng->color_depth)
		{
		case 1:
			return UPNG_LUMINANCE1;
		case 2:
			return UPNG_LUMINANCE2;
		case 4:
			return UPNG_LUMINANCE4;
		case 8:
			return UPNG_LUMINANCE8;
		default:
			return UPNG_BADFORMAT;
		}
	case UPNG_PLT:
		switch (upng->color_depth)
		{
		case 1:
		case 2:
		case 4:
		case 8:
			return UPNG_INDEXED8; /* all indexed depths handled; expanded to RGBA8 on decode */
		default:
			return UPNG_BADFORMAT;
		}
	case UPNG_RGB:
		switch (upng->color_depth)
		{
		case 8:
			return UPNG_RGB8;
		case 16:
			return UPNG_RGB16;
		default:
			return UPNG_BADFORMAT;
		}
	case UPNG_LUMA:
		switch (upng->color_depth)
		{
		case 1:
			return UPNG_LUMINANCE_ALPHA1;
		case 2:
			return UPNG_LUMINANCE_ALPHA2;
		case 4:
			return UPNG_LUMINANCE_ALPHA4;
		case 8:
			return UPNG_LUMINANCE_ALPHA8;
		default:
			return UPNG_BADFORMAT;
		}
	case UPNG_RGBA:
		switch (upng->color_depth)
		{
		case 8:
			return UPNG_RGBA8;
		case 16:
			return UPNG_RGBA16;
		default:
			return UPNG_BADFORMAT;
		}
	default:
		return UPNG_BADFORMAT;
	}
}


static upng_t *upng_new(void)
{
	upng_t *upng = (upng_t *)GetMemory(sizeof(upng_t)); /* zeroed: no error, no file, no palette */

	upng->color_type = UPNG_RGBA;
	upng->color_depth = 8;
	upng->format = UPNG_RGBA8;
	upng->state = UPNG_NEW;
	return upng;
}

/* open the file; the header is read by upng_header.  NULL if it cannot be opened */
upng_t *upng_new_from_file(char *filename)
{
	upng_t *upng = upng_new();

	AppendDefaultExtension(filename, ".png");
	upng->fnbr = FindFreeFileNbr();
	if (!BasicFileOpen(filename, upng->fnbr, FA_READ))
	{
		FreeMemorySafe((void **)&upng);
		return NULL;
	}
	return upng;
}

#define HEADER_FAIL(code)          \
	do                             \
	{                              \
		SET_ERROR(upng, code);     \
		return upng->error;        \
	} while (0)

/* read the header and the chunks before the image data (the palette and its
   transparency); a format the decoder cannot deliver as RGBA8888 is an error */
upng_error upng_header(upng_t *upng)
{
	unsigned char h[33];

	/* if we have an error state, bail now */
	if (upng->error != UPNG_EOK)
	{
		return upng->error;
	}

	/* if the state is not NEW (meaning we are ready to parse the header), stop now */
	if (upng->state != UPNG_NEW)
	{
		return upng->error;
	}

	/* the PNG signature and the IHDR chunk, which must come first */
	if (!png_read(upng, h, 33) || h[0] != 137 || h[1] != 80 || h[2] != 78 || h[3] != 71 || h[4] != 13 || h[5] != 10 || h[6] != 26 || h[7] != 10)
	{
		HEADER_FAIL(UPNG_ENOTPNG);
	}
	if (MAKE_DWORD_PTR(h + 12) != CHUNK_IHDR)
	{
		HEADER_FAIL(UPNG_EMALFORMED);
	}

	/* read the values given in the header */
	upng->width = MAKE_DWORD_PTR(h + 16);
	upng->height = MAKE_DWORD_PTR(h + 20);
	upng->color_depth = h[24];
	upng->color_type = (upng_color)h[25];

	/* determine our color format: indexed rows are expanded through the palette */
	upng->format = determine_format(upng);
	if (upng->format == UPNG_INDEXED8)
	{
		upng->format = UPNG_RGBA8;
	}
	if (upng->format != UPNG_RGBA8)
	{
		HEADER_FAIL(UPNG_EUNFORMAT);
	}

	/* a size, compression method 0 and filter method 0 (the only ones in the spec) */
	if (upng->width == 0 || upng->height == 0 || h[26] != 0 || h[27] != 0)
	{
		HEADER_FAIL(UPNG_EMALFORMED);
	}

	/* interlace method 0 (spec allows 1, but uPNG does not support it) */
	if (h[28] != 0)
	{
		HEADER_FAIL(UPNG_EUNINTERLACED);
	}

	/* the chunks before the image data */
	for (;;)
	{
		unsigned char c[8];
		unsigned long length;
		unsigned type, i;

		if (!png_read(upng, c, 8))
		{
			HEADER_FAIL(UPNG_EMALFORMED);
		}
		length = MAKE_DWORD_PTR(c);
		type = MAKE_DWORD_PTR(c + 4);
		if (type == CHUNK_IDAT)
		{
			upng->chunk_left = length;
			break;
		}
		if (type == CHUNK_PLTE && length % 3 == 0 && length <= 768)
		{
			/* palette chunk - up to 256 RGB entries, opaque unless tRNS says otherwise */
			upng->palette_entries = length / 3;
			for (i = 0; i < upng->palette_entries; i++)
			{
				if (!png_read(upng, upng->palette[i], 3))
				{
					HEADER_FAIL(UPNG_EMALFORMED);
				}
				upng->palette[i][3] = 255;
			}
			length = 0;
		}
		else if (type == CHUNK_PLTE || type == CHUNK_IEND)
		{
			/* a bad palette, or no image data */
			HEADER_FAIL(UPNG_EMALFORMED);
		}
		else if (type == CHUNK_TRNS && upng->color_type == UPNG_PLT)
		{
			/* transparency chunk - for indexed PNGs, one alpha byte per palette entry */
			unsigned n = (length < upng->palette_entries) ? length : upng->palette_entries;
			upng->trns_entries = n;
			for (i = 0; i < n; i++)
			{
				if (!png_read(upng, &upng->palette[i][3], 1))
				{
					HEADER_FAIL(UPNG_EMALFORMED);
				}
			}
			length -= n;
		}
		else if (upng_chunk_critical(c))
		{
			HEADER_FAIL(UPNG_EUNSUPPORTED);
		}
		/* for other chunks, and tRNS for other color types, skip the data; then the CRC */
		if (!png_skip(upng, length + 4))
		{
			HEADER_FAIL(UPNG_EMALFORMED);
		}
	}

	upng->state = UPNG_HEADER;
	return upng->error;
}

/* inflate the image data as it is read, handing each scanline to row() as
   RGBA8888 with its row number; row() returns 0 when it needs no more */
upng_error upng_decode_rows(upng_t *upng, upng_row_fn row, void *ctx)
{
	png_out out;
	unsigned bpp, final = 0, cmf, flg;
	unsigned long winsize = 32768;

	/* parse the main header, if necessary */
	upng_header(upng);
	if (upng->error != UPNG_EOK || upng->state != UPNG_HEADER)
	{
		return upng->error;
	}

	memset(&out, 0, sizeof(out));
	bpp = upng_get_bpp(upng);
	out.linebytes = (upng->width * bpp + 7) / 8;
	out.bytewidth = (bpp + 7) / 8;
	out.rowlen = out.linebytes + 1;
	/* the window must hold 32 KB of history, and a scanline plus the longest copy */
	while (winsize < out.rowlen + 259)
	{
		winsize <<= 1;
	}
	out.mask = winsize - 1;
	upng->buffer = (unsigned char *)GetMemory(winsize + 2 * out.linebytes + (upng->color_type == UPNG_PLT ? upng->width * 4 : 0));
	out.win = upng->buffer;
	out.cur = out.win + winsize;
	out.prev = out.cur + out.linebytes;
	out.rgba = out.prev + out.linebytes;
	out.row = row;
	out.ctx = ctx;

	/* the zlib header: deflate with a window of at most 32 KB and no preset dictionary */
	cmf = read_bits(upng, 8);
	flg = read_bits(upng, 8);
	if (upng->error == UPNG_EOK && ((cmf * 256 + flg) % 31 != 0 || (cmf & 15) != 8 || (cmf >> 4) > 7 || (flg & 32)))
	{
		SET_ERROR(upng, UPNG_EMALFORMED);
	}

	/* the deflate blocks, until the last one or the last row */
	while (!final && !out.done && upng->error == UPNG_EOK)
	{
		unsigned btype;
		final = read_bits(upng, 1);
		btype = read_bits(upng, 2);
		if (btype == 0)
		{
			inflate_stored(upng, &out); /*no compression */
		}
		else if (btype == 3)
		{
			SET_ERROR(upng, UPNG_EMALFORMED);
		}
		else
		{
			inflate_huffman(upng, &out, btype); /*compression, btype 01 or 10 */
		}
	}

	/* the data ended before the last row */
	if (upng->error == UPNG_EOK && !out.done)
	{
		SET_ERROR(upng, UPNG_EMALFORMED);
	}
	FreeMemorySafe((void **)&upng->buffer);
	upng->state = UPNG_DECODED;
	return upng->error;
}

/* PicoMite: if decoding failed, free the decoder and report the error */
void upng_error_check(upng_t *upng)
{
	upng_error err = upng->error;

	if (err == UPNG_EOK)
	{
		return;
	}
	upng_free(upng);
	if (err == UPNG_EUNFORMAT)
	{
		error("Invalid format, must be RGBA8888 or indexed PNG");
	}
	error(err == UPNG_EUNINTERLACED ? "Interlaced PNG not supported" : "Invalid or damaged PNG file");
}

void upng_free(upng_t *upng)
{
	if (upng->fnbr)
	{
		FileClose(upng->fnbr);
	}
	FreeMemorySafe((void **)&upng->buffer);
	FreeMemorySafe((void **)&upng);
}

upng_error upng_get_error(const upng_t *upng)
{
	return upng->error;
}

unsigned upng_get_error_line(const upng_t *upng)
{
	return upng->error_line;
}

unsigned upng_get_width(const upng_t *upng)
{
	return upng->width;
}

unsigned upng_get_height(const upng_t *upng)
{
	return upng->height;
}

unsigned upng_get_bpp(const upng_t *upng)
{
	return upng_get_bitdepth(upng) * upng_get_components(upng);
}

unsigned upng_get_components(const upng_t *upng)
{
	switch (upng->color_type)
	{
	case UPNG_LUM:
		return 1;
	case UPNG_PLT:
		return 1;
	case UPNG_RGB:
		return 3;
	case UPNG_LUMA:
		return 2;
	case UPNG_RGBA:
		return 4;
	default:
		return 0;
	}
}

unsigned upng_get_bitdepth(const upng_t *upng)
{
	return upng->color_depth;
}

unsigned upng_get_pixelsize(const upng_t *upng)
{
	unsigned bits = upng_get_bitdepth(upng) * upng_get_components(upng);
	bits += bits % 8;
	return bits;
}

upng_format upng_get_format(const upng_t *upng)
{
	return upng->format;
}
