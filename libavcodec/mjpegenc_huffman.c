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

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // TODO(yingted): Remove malloc
#include "libavutil/error.h"
#include "mjpegenc_huffman.h"

typedef struct PTable {
	int value;
	int prob;
} PTable;

typedef struct List {
	int nitems; 			// number of items in item_idx and probability		ex. 4
	int item_idx[515];		// index range on the actual items       			    0, 2, 5, 9, 13
	int probability[514];	// probability of each item                 			3, 8, 18, 46   
	int items[257 * 16];	// chain of all items									A, B, A, B, C, A, B, C, D, C, D, D, E
} List;

typedef struct HuffTable {
	int value;
	int length;
} HuffTable;

int compare_by_prob(const void *a, const void *b) {
	PTable a_val = *(PTable *)a;
	PTable b_val = *(PTable *)b;
	if (a_val.prob < b_val.prob) { return -1; }
	if (a_val.prob > b_val.prob) { return 1; }
	return 0;
}

int compare_by_length(const void *a, const void *b) {
	HuffTable a_val = *(HuffTable *)a;
	HuffTable b_val = *(HuffTable *)b;
	if (a_val.length < b_val.length) { return -1; }
	if (a_val.length > b_val.length) { return 1; }
	return 0;
}

void buildHuffmanTree(PTable *probTable, HuffTable* distincts, int size) {
	List *to = malloc(sizeof(List));
	List *from = malloc(sizeof(List));
	List *temp;
	to->nitems = 0;
	from->nitems = 0;
	to->item_idx[0] = 0;
	from->item_idx[0] = 0;
	PTable sorted[size];		// *sorted[i] is the count, sorted[i] - val_counts is the index
	int i;
	for (i = 0; i < size; i++) {
		sorted[i] = probTable[i]; 
	}
	qsort(sorted, size, sizeof(PTable), compare_by_prob);

	int times;
	for (times = 0; times <= 16; times++) {
		to->nitems = 0;
	//	from->nitems = 0;
		to->item_idx[0] = 0;
	//	from->item_idx[0] = 0;
		int j = 0, k = 0;

		if (times < 3) { i = 0; }
		while (i < size || j + 1 < from->nitems) {
			++to->nitems;
			to->item_idx[to->nitems] = to->item_idx[to->nitems - 1];
			if (i < size && (j + 1 >= from->nitems || sorted[i].prob < from->probability[j] + from->probability[j + 1])) {
				to->items[to->item_idx[to->nitems]++] = sorted[i].value;
				to->probability[to->nitems - 1] = sorted[i].prob;
				++i;
			} else {
				for (k = from->item_idx[j]; k < from->item_idx[j + 2]; ++k) {
					to->items[to->item_idx[to->nitems]++] = from->items[k];
				}
				to->probability[to->nitems-1] = from->probability[j] + from->probability[j + 1];
				j += 2;
			}
		}
		temp = to;
		to = from;
		from = temp;
	}

	int nbits[257] = {0};

	for (i = 0; i < from->item_idx[size - 1]; ++i) {
		++nbits[from->items[i]];
	}

	// we don't want to return the 256 bit count (it was just in here to prevent all 1s encoding)
	int j = 0;
	for (i = 0; i < 256; i++) {
		if (nbits[i] > 0) {
			distincts[j].value = i;
			distincts[j].length = nbits[i];
			++j;
		}
	}

	free(to);
	free(from);
}

void ff_mjpeg_encode_huffman_init(MJpegEncHuffmanContext *s) {
    memset(s->val_count, 0, sizeof(s->val_count));
}

int ff_mjpeg_encode_huffman_close(MJpegEncHuffmanContext *s,
        uint8_t bits[17], uint8_t val[], int max_nval) {
    int i;
    int nval = 0;
    for (i=0;i<256;++i) {
        if (s->val_count[i])
            ++nval;
    }
    if (nval > max_nval) {
        return AVERROR(EINVAL);
    }

#if 1
	PTable val_counts[nval + 1];
	int j = 0;
	for (i = 0; i < nval; i++) {
		if (s->val_count[i]) {
			val_counts[j].value = i;
			val_counts[j].prob = s->val_count[i];
		}
	}
	val_counts[j].value = 256;
	val_counts[j].prob = 0;
	HuffTable distincts[nval];
	buildHuffmanTree(val_counts, distincts, nval + 1);

	qsort(distincts, nval, sizeof(HuffTable), compare_by_length);
#endif

    // TODO(yingted): Use package merge results
    memset(bits, 0, sizeof(bits[0]) * 17);
#if 1
    for (i = 0; i < nval; i++) {
		val[i] = distincts[i].code;
		++bits[distincts[i].length];
	}

    // bits[8] = nval > 255 ? 255 : nval;
    // bits[9] = nval - bits[8];
    // nval = 0;
    // for (i = 0; i < 256; ++i) {
    //     if (s->val_count[i])
    //         val[nval++] = i;
    // }
#else
    nval = max_nval;
    bits[8] = nval > 255 ? 255 : nval;
    bits[9] = nval - bits[8];
    for (i = 0; i < 256; ++i) {
        val[i] = i;
    }
#endif

    return 0;
}
