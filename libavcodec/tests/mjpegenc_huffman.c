/*
 * Copyright (c) 2016 William Ma, Sofia Kim, Dustin Woo
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

// Test the example given on @see <a
// href="http://guru.multimedia.cx/small-tasks-for-ffmpeg/">Small Tasks</a>
int main(int argc, char **argv) {
    int i, ret = 0;
    // Probabilities of symbols 0..4
    PTable val_counts[] = {
        {.value = 0, .prob = 1},
        {.value = 1, .prob = 2},
        {.value = 2, .prob = 5},
        {.value = 3, .prob = 10},
        {.value = 4, .prob = 21},
    };
    // Expected code lengths for each symbol
    HuffTable expected[] = {
        {.code = 0, .length = 3},
        {.code = 1, .length = 3},
        {.code = 2, .length = 3},
        {.code = 3, .length = 3},
        {.code = 4, .length = 1},
    };
    // Actual code lengths
    HuffTable distincts[5];

    // Build optimal huffman tree
    ff_mjpegenc_huffman_compute_bits(val_counts, distincts, 5);

    for (i = 0; i < 5; i++) {
        if (distincts[i].code != expected[i].code ||
            distincts[i].length != expected[i].length) {
            fprintf(stderr,
                "Built huffman does not equal expectations. "
                "Expected: code %d probability %d, "
                "Actual: code %d probability %d\n",
                expected[i].code, expected[i].length,
                distincts[i].code, distincts[i].length);
            ret = 1;
        }
    }

    return ret;
}
