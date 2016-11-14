/*
 * MJPEG encoder
 * Copyright (c) 2016 William Ma, Ted Ying
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
 * Huffman table generation for MJPEG encoder.
 */

#ifndef AVCODEC_MJPEGENC_HUFFMAN_H
#define AVCODEC_MJPEGENC_HUFFMAN_H

typedef struct MJpegEncHuffmanContext {
    int val_count[256];
} MJpegEncHuffmanContext;

void ff_mjpeg_encode_huffman_init(MJpegEncHuffmanContext *s);
static inline void ff_mjpeg_encode_huffman_increment(
        MJpegEncHuffmanContext *s, uint8_t val) {
    ++s->val_count[val];
}
int ff_mjpeg_encode_huffman_close(MJpegEncHuffmanContext *s,
        uint8_t bits[17], uint8_t val[], int max_nval);

// For tests:

typedef struct PTable {
    int value;
    int prob;
} PTable;

typedef struct List {
    int nitems;             // number of items in item_idx and probability      ex. 4
    int item_idx[515];      // index range on the actual items                  0, 2, 5, 9, 13
    int probability[514];   // probability of each item                         3, 8, 18, 46
    int items[257 * 16];    // chain of all items                               A, B, A, B, C, A, B, C, D, C, D, D, E
} List;

typedef struct HuffTable {
    int code;
    int length;
} HuffTable;

void ff_mjpegenc_huffman_compute_bits(PTable *prob_table, HuffTable *distincts, int size);
#endif /* AVCODEC_MJPEGENC_HUFFMAN_H */
