/*
 * (c) 2016 some 2018 Software Engineering UWaterloo students
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Optimal Huffman Encoding tests.
 */

#include "libavcodec/avcodec.h"
#include <stdlib.h>
#include "libavcodec/mjpegenc.h"
#include "libavcodec/mjpegenc_huffman.h"
#include "libavcodec/mjpegenc_common.h"
#include "libavcodec/mpegvideo.h"

//Test the example given on @see <a href="http://guru.multimedia.cx/small-tasks-for-ffmpeg/">Small Tasks</a>
int main(int argc, char **argv)
{
	int i;
	PTable *val_counts;
	HuffTable *distincts;
	HuffTable *expected;

	val_counts = malloc(5 * sizeof(PTable));
	distincts = malloc(5 * sizeof(HuffTable));
	expected = malloc(5 * sizeof(HuffTable));

	//Init example probability table
	val_counts[0].value = 0;
	val_counts[0].prob = 1;
	val_counts[1].value = 1;
	val_counts[1].prob = 2;
	val_counts[2].value = 2;
	val_counts[2].prob = 5;
	val_counts[3].value = 3;
	val_counts[3].prob = 10;
	val_counts[4].value = 4;
	val_counts[4].prob = 21;

	//Create expected Huffman Table
	expected[0].code = 0;
	expected[0].length = 3;
	expected[1].code = 1;
	expected[1].length = 3;
	expected[2].code = 1;
	expected[2].length = 3;
	expected[3].code = 1;
	expected[3].length = 3;
	expected[4].code = 0;
	expected[4].length = 1;

	//Build optimal huffman tree
	ff_mjpegenc_huffman_compute_bits(val_counts, distincts, 5);

	printf("Test");
	for (i = 0; i < 5; i++) {
		if (distincts[i].code != expected[i].code || distincts[i].length != expected[i].length) {
			printf("Built huffman does not equal expectations. Expected: code %d probability %d, Actual: code %d probability %d", 
				distincts[i].code, distincts[i].length,
				expected[i].code, expected[i].length);
		}
	}

	free(val_counts);
	free(distincts);
	free(expected);

	return 0;
}
