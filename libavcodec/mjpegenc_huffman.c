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

typedef struct Symbol {
	int value;
	struct Symbol* next;
} Symbol;

static Symbol* copySymbols(Symbol* root, Symbol ***tail) {
	// Copy root into root2
	Symbol *root2 = NULL;
	*tail = &root2;
	printf("copying: ");
	while (root) {
		// Append to tail
		**tail = malloc(sizeof(Symbol));
		(**tail)->value = root->value;
		printf("%d ", root->value);
		// Advance root and tail
		*tail = &(**tail)->next;
		root = root->next;
		
	}
	printf("\n");
	return root2;
}

static void printSymbols(Symbol* root) {
	Symbol* sym = root;
	printf("TEST: ");
	while (sym) {
		printf("%d ", sym->value);
		sym = sym->next;
	}
	printf("\n");
}

// DEEP COPIES THE NODE + ALL ITS NEXT NODES
static Symbol* concatSymbol(Symbol* root, Symbol* node) {
	if (!root) { return node; }
	// Append node to the tail of root2
	Symbol **tail = NULL, **tail2 = NULL;
	Symbol *rootCopy = copySymbols(root, &tail);
	*tail = copySymbols(node, &tail2);
	(*tail)->next = NULL;
	printSymbols(*tail);
	return rootCopy;
}

typedef struct PTable {
	Symbol* value;
	int prob;
} PTable;

typedef struct Occurence {
	int value;
	int occurences;
} Occurence;

typedef struct Heap {
	int size;
	int count;
	PTable** value;
} Heap;

static Heap* initHeap(int size) {
	Heap* heap = malloc(sizeof(Heap));
	heap->size = size;
	heap->count = 0;
	heap->value = malloc(sizeof(PTable*) * size);
	return heap;
}

static void pushHeap(Heap* heap, PTable* value) {
	int curr = heap->count;
	int parent;

	while (curr) {
		parent = (curr - 1) >> 1;
		if (heap->value[parent]->prob < value->prob) { break; }
		heap->value[curr] = heap->value[parent];
		curr = parent;
	}

	heap->value[curr] = value;
	heap->count++;
}

static PTable* popHeap(Heap* heap) {
	int curr = 0;
	int swap1, swap2;

	if (heap->count <= 0) { return NULL; }
	PTable* top = heap->value[0];
	PTable* swapper = heap->value[heap->count - 1];

	for (;;) {
		swap1 = (curr << 1) + 1;
		if (swap1 >= heap->count) { break; }
		swap2 = swap1 + 1;
		if (swap2 < heap->count && heap->value[swap2]->prob < heap->value[swap1]->prob) {
			swap1 = swap2;
		} else if (swapper->prob < heap->value[swap1]->prob) {
			break;
		}
		heap->value[curr] = heap->value[swap1];
		curr = swap1;
	}
	heap->value[curr] = swapper;
	heap->count--;
	return top;
}

static Heap* heapify(PTable** value, int size) {
	Heap* heap = initHeap(size);
	int i, level, curr, swap1, swap2;
	PTable* swapper = 0;

	// deep copy the array
	PTable* copy;
	for (i = 0; i < size; i++) {
		copy = malloc(sizeof(PTable));
		copy->value = malloc(sizeof(Symbol));
		copy->value->value = value[i]->value->value;
		copy->value->next = NULL;
		copy->prob = value[i]->prob;
		heap->value[i] = copy;
	}
	heap->count = size;

	level = (size >> 1) - 1;
	for (;;) {
		curr = level;
		swapper = heap->value[level];

		for (;;) {
			swap1 = (curr << 1) + 1;
			if (swap1 >= size) { break; }
			swap2 = swap1 + 1;
			if (swap2 < size && heap->value[swap2]->prob < heap->value[swap1]->prob) {
				swap1 = swap2;
			} else if (swapper->prob < heap->value[swap1]->prob) {
				break;
			}

			heap->value[curr] = heap->value[swap1];
			curr = swap1;
		}

		if (curr != level) { heap->value[curr] = swapper; }

		if (!level) { break; }
		level--;
	}

	return heap;
}

static PTable** constructProbTable(int* input, int size) {
	int* probtable = (int*) malloc(sizeof(int)*256);
	// initialize to zero
	int i;
	for (i = 0; i < 256; ++i) {
		probtable[i] = 0;
	}
	// count each symbol
	for (i = 0; i < size; ++i) {
		int index = input[i];
		probtable[index]++;
	}
	// count distinct symbol
	int distinctsize = 0;
	for (i = 0; i < 256; ++i) {
		if (probtable[i] != 0) {
			distinctsize++;
		}
	}
	PTable** ptables = (PTable**) malloc(sizeof(PTable*) * (distinctsize + 1));
	int pTableindex = 0;
	for (i = 0; i < 256; ++i) {
		if (probtable[i] != 0) {
			PTable* ptable = (PTable*) malloc(sizeof(PTable));
			Symbol* psymbol = (Symbol*) malloc(sizeof(Symbol));
			psymbol->value = i;
			psymbol->next = 0;
			ptable->value = psymbol;
			ptable->prob = probtable[i];
			ptables[pTableindex] = ptable;
			pTableindex++;
		}
	}
	free(probtable);

	PTable* ptable = (PTable*) malloc(sizeof(PTable));
	Symbol* psymbol = (Symbol*) malloc(sizeof(Symbol));
	psymbol->value = -1;
	psymbol->next = 0;
	ptable->value = psymbol;
	ptable->prob = 0;
	ptables[pTableindex] = ptable;
	return ptables;
}

static void printSymbol(PTable* table) {
	Symbol* sym = table->value;
	printf("Symbol: ");
	while (sym) {
		printf("%d ", sym->value);
		sym = sym->next;
	}
	printf("\n");
}

static void buildHuffmanTree(PTable** variables, int size) {
	int i, j;

	// step 2
	Heap* heap = heapify(variables, size);
	Heap* heap2;
	for(i = 0; i < 3; i++) {	
		heap2 = initHeap(256);
		PTable *combine1, *combine2, *newPTable;
		// step 3-4
		while (heap->count > 1) {
			printf("heapcount: %d\n", heap->count);
			combine1 = popHeap(heap);
			combine2 = popHeap(heap);
			newPTable = malloc(sizeof(PTable));
			newPTable->value = NULL;
			newPTable->prob = combine1->prob + combine2->prob;
			printSymbol(combine1);
			printSymbol(combine2);
			newPTable->value = concatSymbol(newPTable->value, combine1->value);
			newPTable->value = concatSymbol(newPTable->value, combine2->value);
			printSymbol(newPTable);
			pushHeap(heap2, newPTable);
		}
		// step 2
		for (j = 0; j < size; j++) {
			newPTable = malloc(sizeof(PTable));
			newPTable->prob = variables[j]->prob;
			newPTable->value = malloc(sizeof(Symbol));
			newPTable->value->next = NULL;
			newPTable->value->value = variables[j]->value->value;
			pushHeap(heap2, newPTable);
		}
		printf("%d\n", i);
		heap = heap2;
	}
printf("im out\n");
	// Occurence *occurences[size];
	PTable *node = popHeap(heap2);
	Symbol* values;
	while(node) {
		values = node->value;
		while (values) {
			printf("%d ", values->value);
			values = values->next;
		}
		printf("\n");
		node = popHeap(heap2);
	}
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

	PTable** result = constructProbTable(s->val_count, 256);
	buildHuffmanTree(result, 16);
	// Heap* heap = heapify(result, 8);
	// \ int i;
	// PTable* val = popHeap(heap);
	// do {
	// 	printf("%d, %d\n", val->value->value, val->prob);
	// 	free(val->value);
	// 	free(val);
	// 	val = popHeap(heap);
	// } while (val);

	for (i = 0; i < 16; i++) {
		free(result[i]->value);
		free(result[i]);
	}
	free(result);
//	free(heap->value);
	//free(heap);

    // TODO(yingted): Use package merge results
    memset(bits, 0, sizeof(bits[0]) * 17);
    bits[8] = nval > 255 ? 255 : nval;
    bits[9] = nval - bits[8];
    for (i = 0; i < nval; ++i) {
        val[i] = i;
    }

    return 0;
}
