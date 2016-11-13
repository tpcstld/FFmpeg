#include <stdio.h>
#include <stdlib.h>

// typedef struct Symbol {
// 	int value;
// 	struct Symbol* next;
// } Symbol;

// Symbol* copySymbols(Symbol* root, Symbol ***tail) {
// 	// Copy root into root2
// 	Symbol *root2 = NULL;
// 	*tail = &root2;
// 	printf("copying: ");
// 	while (root) {
// 		// Append to tail
// 		**tail = malloc(sizeof(Symbol));
// 		(**tail)->value = root->value;
// 		printf("%d ", root->value);
// 		// Advance root and tail
// 		*tail = &(**tail)->next;
// 		root = root->next;
		
// 	}
// 	**tail = NULL;
// 	printf("\n");
// 	return root2;
// }

// void printSymbols(Symbol* root) {
// 	Symbol* sym = root;
// 	printf("TEST: ");
// 	while (sym) {
// 		printf("%d ", sym->value);
// 		sym = sym->next;
// 	}
// 	printf("\n");
// }

// // DEEP COPIES THE NODE + ALL ITS NEXT NODES
// Symbol* concatSymbol(Symbol* root, Symbol* node) {
// 	if (!root) { return node; }
// 	// Append node to the tail of root2
// 	Symbol **tail = NULL, **tail2 = NULL;
// 	Symbol *rootCopy = copySymbols(root, &tail);
// 	*tail = copySymbols(node, &tail2);
// 	//(*tail)->next = NULL;
// 	printSymbols(*tail);
// 	return rootCopy;
// }

typedef struct PTable {
	//Symbol* value; // int value
	int value;
	int prob;
} PTable;

// typedef struct Occurence {
// 	int value;
// 	int occurences;
// } Occurence;

// typedef struct Heap {
// 	int size;
// 	int count;
// 	PTable** value;
// } Heap;

// Heap* initHeap(int size) {
// 	Heap* heap = malloc(sizeof(Heap));
// 	heap->size = size;
// 	heap->count = 0;
// 	heap->value = malloc(sizeof(PTable*) * size);
// 	return heap;
// }

// void pushHeap(Heap* heap, PTable* value) {
// 	int curr = heap->count;
// 	int parent;

// 	while (curr) {
// 		parent = (curr - 1) >> 1;
// 		if (heap->value[parent]->prob < value->prob) { break; }
// 		heap->value[curr] = heap->value[parent];
// 		curr = parent;
// 	}

// 	heap->value[curr] = value;
// 	heap->count++;
// }

// PTable* popHeap(Heap* heap) {
// 	int curr = 0;
// 	int swap1, swap2;

// 	if (heap->count <= 0) { return NULL; }
// 	PTable* top = heap->value[0];
// 	PTable* swapper = heap->value[heap->count - 1];

// 	for (;;) {
// 		swap1 = (curr << 1) + 1;
// 		if (swap1 >= heap->count) { break; }
// 		swap2 = swap1 + 1;
// 		if (swap2 < heap->count && heap->value[swap2]->prob < heap->value[swap1]->prob) {
// 			swap1 = swap2;
// 		} else if (swapper->prob < heap->value[swap1]->prob) {
// 			break;
// 		}
// 		heap->value[curr] = heap->value[swap1];
// 		curr = swap1;
// 	}
// 	heap->value[curr] = swapper;
// 	heap->count--;
// 	return top;
// }

// Heap* heapify(PTable** value, int size) {
// 	Heap* heap = initHeap(size);
// 	int i, level, curr, swap1, swap2;
// 	PTable* swapper = 0;

// 	// deep copy the array
// 	PTable* copy;
// 	for (i = 0; i < size; i++) {
// 		copy = malloc(sizeof(PTable));
// 		copy->value = malloc(sizeof(Symbol));
// 		copy->value->value = value[i]->value->value;
// 		copy->value->next = NULL;
// 		copy->prob = value[i]->prob;
// 		heap->value[i] = copy;
// 	}
// 	heap->count = size;

// 	level = (size >> 1) - 1;
// 	for (;;) {
// 		curr = level;
// 		swapper = heap->value[level];

// 		for (;;) {
// 			swap1 = (curr << 1) + 1;
// 			if (swap1 >= size) { break; }
// 			swap2 = swap1 + 1;
// 			if (swap2 < size && heap->value[swap2]->prob < heap->value[swap1]->prob) {
// 				swap1 = swap2;
// 			} else if (swapper->prob < heap->value[swap1]->prob) {
// 				break;
// 			}

// 			heap->value[curr] = heap->value[swap1];
// 			curr = swap1;
// 		}

// 		if (curr != level) { heap->value[curr] = swapper; }

// 		if (!level) { break; }
// 		level--;
// 	}

// 	return heap;
// }

// PTable** constructProbTable(int* input, int size) {
// 	int* probtable = (int*) malloc(sizeof(int)*256);
// 	// initialize to zero
// 	int i;
// 	for (i = 0; i < 256; ++i) {
// 		probtable[i] = 0;
// 	}
// 	// count each symbol
// 	for (i = 0; i < size; ++i) {
// 		int index = input[i];
// 		probtable[index]++;
// 	}
// 	// count distinct symbol
// 	int distinctsize = 0;
// 	for (i = 0; i < 256; ++i) {
// 		if (probtable[i] != 0) {
// 			distinctsize++;
// 		}
// 	}
// 	PTable** ptables = (PTable**) malloc(sizeof(PTable*) * (distinctsize + 1));
// 	int pTableindex = 0;
// 	for (i = 0; i < 256; ++i) {
// 		if (probtable[i] != 0) {
// 			PTable* ptable = (PTable*) malloc(sizeof(PTable));
// 			Symbol* psymbol = (Symbol*) malloc(sizeof(Symbol));
// 			psymbol->value = i;
// 			psymbol->next = 0;
// 			ptable->value = psymbol;
// 			ptable->prob = probtable[i];
// 			ptables[pTableindex] = ptable;
// 			pTableindex++;
// 		}
// 	}
// 	free(probtable);

// 	PTable* ptable = (PTable*) malloc(sizeof(PTable));
// 	Symbol* psymbol = (Symbol*) malloc(sizeof(Symbol));
// 	psymbol->value = -1;
// 	psymbol->next = 0;
// 	ptable->value = psymbol;
// 	ptable->prob = 0;
// 	ptables[pTableindex] = ptable;
// 	return ptables;
// }

// void printSymbol(PTable* table) {
// 	Symbol* sym = table->value;
// 	printf("Symbol: ");
// 	while (sym) {
// 		printf("%d ", sym->value);
// 		sym = sym->next;
// 	}
// 	printf("\n");
// }

// void buildHuffmanTree(PTable** variables, int size) {
// 	int i, j;

// 	// step 2
// 	Heap* heap = heapify(variables, size);
// 	Heap* heap2;
// 	for(i = 0; i < 3; i++) {	
// 		heap2 = initHeap(256);
// 		PTable *combine1, *combine2, *newPTable;
// 		// step 3-4
// 		while (heap->count > 1) {
// 			printf("heapcount: %d\n", heap->count);
// 			combine1 = popHeap(heap);
// 			combine2 = popHeap(heap);
// 			newPTable = malloc(sizeof(PTable));
// 			newPTable->value = NULL;
// 			newPTable->prob = combine1->prob + combine2->prob;
// 			printSymbol(combine1);
// 			printSymbol(combine2);
// 			newPTable->value = concatSymbol(newPTable->value, combine1->value);
// 			newPTable->value = concatSymbol(newPTable->value, combine2->value);
// 			printSymbol(newPTable);
// 			pushHeap(heap2, newPTable);
// 		}
// 		// step 2
// 		for (j = 0; j < size; j++) {
// 			newPTable = malloc(sizeof(PTable));
// 			newPTable->prob = variables[j]->prob;
// 			newPTable->value = malloc(sizeof(Symbol));
// 			newPTable->value->next = NULL;
// 			newPTable->value->value = variables[j]->value->value;
// 			pushHeap(heap2, newPTable);
// 		}
// 		printf("%d\n", i);
// 		heap = heap2;
// 	}
// printf("im out\n");
// 	// Occurence *occurences[size];
// 	PTable *node = popHeap(heap2);
// 	Symbol* values;
// 	while(node) {
// 		values = node->value;
// 		while (values) {
// 			printf("%d ", values->value);
// 			values = values->next;
// 		}
// 		printf("\n");
// 		node = popHeap(heap2);
// 	}
// }

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

int compare_by_value(const void *a, const void *b) {
	PTable a_val = *(PTable *)a;
	PTable b_val = *(PTable *)b;
	if (a_val.prob < b_val.prob) { return -1; }
	if (a_val.prob > b_val.prob) { return 1; }
	return 0;
}

void buildHuffmanTree(PTable probTable[], int size) {
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
	qsort(sorted, size, sizeof(PTable), compare_by_value);

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

	HuffTable distincts[size];
	int j = 0;
	for (i = 0; i < 257; i++) {
		if (nbits[i] > 0) {
			distincts[j].value = i;
			distincts[j].length = nbits[i];
			++j;
		}
	}

	free(to);
	free(from);
}

int main() {
	int input[39];
	// input[0] = 100;
	// input[1] = 100;
	// input[2] = 200;
	// input[3] = 1;
	// input[4] = 1;
	// input[5] = 1;
	// input[6] = 2;
	// input[7] = 5;
	// input[8] = 10;
	// input[9] = 20;
	// input[10] = 5;
	// input[11] = 5;
	// input[12] = 5;
	// input[13] = 5;
	// input[14] = 5;
	// input[15] = 5;
	// input[19] = 5;
	// input[16] = 20;
	// input[17] = 20;
	// input[18] = 20;
	int i;
	for (i = 0; i < 10; i++) {
		input[i] = 4;
	}
	input[i++] = 1;
	input[i++] = 2;
	input[i++] = 2;
	for (i; i < 18; i++) {
		input[i] = 3;
	}
	for (i; i < 39; i++) {
		input[i] = 5;
	}
	// 8 5s (A), 4 20s (B), 3 1s (C), 2 100s (D), 1 2 (E), 1 10 (F), 1 200 (G), 1 -1 (H)

	// H1 F1 G1 E1 D2 C3 B4 A8
	// H1 F1 HF2 G1 E1 GE2 D2 C3 DC5 B4 A8 BA12

	// 5 -1s, 3 2s, 2 200s, 2 5s, 4 100s, 4 10s, 2 20s, 3 1s
	// 2 5s, 2 20s, 3 1s, 4 100s, 3 2s, 4 10s, 2 200s, 5 -1s
	int counts[256] = {0};
	int size = 39;
	// count each symbol
	for (i = 0; i < size; ++i) {
		counts[input[i]]++;
	}

	int distincts = 0;
	for (i = 0; i < 256; i++) {
		if (counts[i] > 0) { ++distincts; }
	}

	PTable probTable[distincts + 1];
	int j = 0;
	for (i = 0; i < 256; i++) {
		if (counts[i] > 0) {
			probTable[j].value = i;
			probTable[j].prob = counts[i];
			++j;
		}	 
	}
	probTable[j].value = 256;
	probTable[j].prob = 0;

	//PTable** result = constructProbTable(input, 39);
	buildHuffmanTree(probTable, distincts + 1);

	// Heap* heap = heapify(result, 8);
	// \ int i;
	// PTable* val = popHeap(heap);
	// do {
	// 	printf("%d, %d\n", val->value->value, val->prob);
	// 	free(val->value);
	// 	free(val);
	// 	val = popHeap(heap);
	// } while (val);

	// free(input);

	// for (i = 0; i < 5; i++) {
	// 	free(result[i]->value);
	// 	free(result[i]);
	// }
	// free(result);
//	free(heap->value);
	//free(heap);
	return 0;
}