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
#include <assert.h>
#include <stdlib.h> // TODO(yingted): Remove malloc
#include "libavutil/error.h"
#include "mjpegenc_huffman.h"

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

	int times, i, j, k;

	int nbits[257] = {0};

	int min;

	to->nitems = 0;
	from->nitems = 0;
	to->item_idx[0] = 0;
	from->item_idx[0] = 0;
	qsort(probTable, size, sizeof(PTable), compare_by_prob);
	for (times = 0; times <= 16; times++) {
		to->nitems = 0;
		to->item_idx[0] = 0;

		j = 0;
		k = 0;

		if (times < 16) { i = 0; }
		while (i < size || j + 1 < from->nitems) {
			++to->nitems;
			to->item_idx[to->nitems] = to->item_idx[to->nitems - 1];
			if (i < size && (j + 1 >= from->nitems || probTable[i].prob < from->probability[j] + from->probability[j + 1])) {
				to->items[to->item_idx[to->nitems]++] = probTable[i].value;
				to->probability[to->nitems - 1] = probTable[i].prob;
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

		printf("round end\n");
	}

	min = (size - 1 < from->nitems) ? size - 1 : from->nitems;
	for (i = 0; i < from->item_idx[min]; ++i) {
		++nbits[from->items[i]];
	}
	// we don't want to return the 256 bit count (it was just in here to prevent all 1s encoding)
	j = 0;
	for (i = 0; i < 256; i++) {
		if (nbits[i] > 0) {
			distincts[j].code = i;
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
    int i, j;
    int nval = 0;
    PTable *val_counts;
    HuffTable *distincts;

    for (i=0;i<256;++i) {
        if (s->val_count[i])
            ++nval;
    }
    if (nval > max_nval) {
        return AVERROR(EINVAL);
    }

	val_counts = malloc((nval + 1)*sizeof(PTable));
	j = 0;
	for (i = 0; i < 256; ++i) {
		if (s->val_count[i]) {
			val_counts[j].value = i;
			val_counts[j].prob = s->val_count[i];
			++j;
		}
	}
	val_counts[j].value = 256;
	val_counts[j].prob = 0;
	distincts = malloc(nval*sizeof(HuffTable));
	buildHuffmanTree(val_counts, distincts, nval + 1);
	qsort(distincts, nval, sizeof(HuffTable), compare_by_length);

    memset(bits, 0, sizeof(bits[0]) * 17);
    for (i = 0; i < nval; i++) {
		val[i] = distincts[i].code;
		++bits[distincts[i].length];
	}

	free(val_counts);
	free(distincts);
    return 0;
}
