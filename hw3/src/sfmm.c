/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"


int find_freelist_start(size_t size) {
	if (size == 32) {
		return 0;
	}
	int index = 1;
	size_t M = 32;
	while (index < NUM_FREE_LISTS - 1) {
		if (size > M && size <= 2 * M) {
			return index;
		}
		M = 2 * M;
		index++;
	}
	return NUM_FREE_LISTS - 1;
}

struct sf_block *split_freelist_block(size_t size, sf_block* node) {
	node->header ^= MAGIC;
	node->header |= THIS_BLOCK_ALLOCATED;
	size_t block_size = (node->header >> 3) << 3;
	if (block_size - size >= 32) { // Split
		size_t remainder_size = block_size - size;
		node->header = size | (node->header & PREV_BLOCK_ALLOCATED);
		node->header |= THIS_BLOCK_ALLOCATED;
		node->header ^= MAGIC;
		struct sf_block* remainder_block = (struct sf_block*)((size_t*)node + (size / sizeof(size_t)));
		remainder_block->header = remainder_size | PREV_BLOCK_ALLOCATED;
		remainder_block->header ^= MAGIC;
		struct sf_block* remainder_footer = (struct sf_block*)((size_t*)remainder_block + (remainder_size / sizeof(size_t)));
		remainder_footer->prev_footer = remainder_block->header;
		if ((size_t*)remainder_footer == (size_t*)sf_mem_end()-16) {
			remainder_footer->header = 0 ^ MAGIC;
		}
		int index = find_freelist_start(remainder_size); // Insert remainder back to freelist
		remainder_block->body.links.next = sf_free_list_heads[index].body.links.next;
		sf_free_list_heads[index].body.links.next = remainder_block;
		remainder_block->body.links.prev = &sf_free_list_heads[index];
    	remainder_block->body.links.next->body.links.prev = remainder_block;
		return node;
	}
	node->header ^= MAGIC;
	struct sf_block* next = (struct sf_block*)((size_t*)node + (block_size / sizeof(size_t)));
	next->header ^= MAGIC;
	next->header |= PREV_BLOCK_ALLOCATED;
	if ((next->header & THIS_BLOCK_ALLOCATED) == 0 && (size_t*)next < (size_t*)sf_mem_end()-16) { // If not allocated, set footer = header;
		block_size = (next->header >>3) << 3;
		struct sf_block* next_footer = (struct sf_block*)((size_t*)next + (block_size / sizeof(size_t)));
		next_footer->prev_footer = next->header ^ MAGIC;
	}
	next->header ^= MAGIC;
	return node;
}

struct sf_block *malloc_freelist(size_t size) {
	int index = find_freelist_start(size);
	while(index < NUM_FREE_LISTS){
		struct sf_block* node = &sf_free_list_heads[index];
		struct sf_block* node_pointer = node->body.links.next;
		if (node != node->body.links.next || node != node->body.links.prev) {
			while (node_pointer != node) {
				if ((((node_pointer->header ^ MAGIC) >> 3) << 3) >= size) {
					struct sf_block* next = node_pointer->body.links.next;
					struct sf_block* prev = node_pointer->body.links.prev;
					next->body.links.prev = prev;
					prev->body.links.next = next;
					node_pointer->body.links.prev = NULL;
					node_pointer->body.links.next = NULL;
					return split_freelist_block(size, node_pointer);
				}
				node_pointer = node_pointer->body.links.next;
			}
		}
		index++;
	}
	return NULL;
}

void initialize_heap() {
	for (int i = 0; i < NUM_QUICK_LISTS; i++) {
		sf_quick_lists[i].length = 0;
	}
	for(int i = 0; i < NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
	sf_mem_grow();
	struct sf_block *heap = (struct sf_block*) sf_mem_start();
	heap->header = PAGE_SZ - 16;
	// struct sf_block *next = heap + (heap->header / sizeof(struct sf_block));
	struct sf_block* next = (struct sf_block*)((size_t*)heap + (heap->header / sizeof(size_t)));
	heap->header ^= MAGIC;
	next->prev_footer = heap->header;
	next->header = 0 ^ MAGIC;
    int index = find_freelist_start(((heap->header ^ MAGIC) >> 3) << 3);
    sf_free_list_heads[index].body.links.next = heap;
    sf_free_list_heads[index].body.links.prev = heap;
    heap->body.links.next = &sf_free_list_heads[index];
    heap->body.links.prev = &sf_free_list_heads[index];
    // sf_show_heap();
}

void *sf_malloc(size_t size) {
	if (sf_mem_start() == sf_mem_end()) {
		initialize_heap();
	}
	if (size == 0) {
		return NULL;
	}
	if ((long)size < 0) {
		sf_errno = ENOMEM;
		return NULL;
	}
	size_t total_size = size + 8;
	if (total_size <= 32) {
		total_size = 32;
	} else if (total_size % 16 != 0) {
		total_size = (total_size / 16 + 1) * 16;
	}
	//sf_show_heap();
	int index = (total_size - 32) / 16;
	// printf("%d\n", sf_quick_lists[0].length);
	if (index >= 0 && index < NUM_QUICK_LISTS && sf_quick_lists[index].length > 0) {
		struct sf_block* allocated = (struct sf_block*)sf_quick_lists[index].first; // extract the first satisfied block
		sf_quick_lists[index].first = allocated->body.links.next; // quicklist links to next
		allocated->body.links.next = NULL;
		sf_quick_lists[index].length--;
		return (void*) allocated->body.payload;
	}
	// sf_show_heap();
	struct sf_block* ans = malloc_freelist(total_size);
	if (ans != NULL) {
		return (void*) ans->body.payload;
	}
	// sf_show_heap();
	while ((ans = malloc_freelist(total_size)) == NULL) {	// TODO: Request more memory
		struct sf_block *current = (struct sf_block*) ((size_t*)sf_mem_end() - 16 / sizeof(size_t));
		if(sf_mem_grow() == NULL) {
			sf_errno = ENOMEM;
    		return NULL;
		}
		current->header ^= MAGIC;
		current->header = PAGE_SZ | (current->header & PREV_BLOCK_ALLOCATED);
		current->header ^= MAGIC;
		struct sf_block *current_footer = (struct sf_block*) ((size_t*)sf_mem_end() - 16 / sizeof(size_t));//(struct sf_block*) ((size_t*)current + ((current->header >> 3) << 3) / sizeof(size_t));
		current_footer->prev_footer =current->header;
		current_footer->header = 0 ^ MAGIC;
		// printf("%s\n", "Here"); // ERROR STARTS HERE
		if (((current->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0) {
			// TODO: Previous block is allocated, coalesce
			size_t new_size = (((current->prev_footer ^ MAGIC) >> 3) << 3) + PAGE_SZ;
			struct sf_block* previous_block = (struct sf_block*) ((size_t*)current - (((current->prev_footer ^ MAGIC) >> 3) << 3) / sizeof(size_t));
			// printf("Previous %p, Current %p\n", previous_block, current);
			previous_block->header = new_size | ((previous_block->header ^ MAGIC) & PREV_BLOCK_ALLOCATED);
			previous_block->header ^= MAGIC;
			current_footer->prev_footer = previous_block->header;
			// Unlink
			struct sf_block* freelist_next = previous_block->body.links.next;
			struct sf_block* freelist_prev = previous_block->body.links.prev;
			freelist_prev->body.links.next = freelist_next;
			freelist_next->body.links.prev = freelist_prev;
			previous_block->body.links.next = NULL;
			previous_block->body.links.prev = NULL;
			// Insert
			int index_new = find_freelist_start(((previous_block->header ^ MAGIC) >> 3) << 3);
			struct sf_block* temp = sf_free_list_heads[index_new].body.links.next;
			previous_block->body.links.next = temp;
			previous_block->body.links.prev = &sf_free_list_heads[index_new];
			sf_free_list_heads[index_new].body.links.next = previous_block;
			temp->body.links.prev = previous_block;
		}
	}
	// sf_errno = ENOMEM;
    return (void*) ans->body.payload;
}

void remove_freelist_block(struct sf_block *block) {
    block->body.links.prev->body.links.next = block->body.links.next;
    block->body.links.next->body.links.prev = block->body.links.prev;
}

void coalesce_insert_freelist(struct sf_block *current) {
	if (current->body.links.next != NULL && current->body.links.prev != NULL) {
		remove_freelist_block(current);
	}
	current->header ^= MAGIC;
	current->header &= ~THIS_BLOCK_ALLOCATED;
	struct sf_block* current_footer = (struct sf_block*) ((size_t*)current + (current->header & ~7) / sizeof(size_t));
	current->header ^= MAGIC;
	current_footer->prev_footer = current->header;
	current_footer->header = (current_footer->header ^ MAGIC) & ~PREV_BLOCK_ALLOCATED;
	current_footer->header ^= MAGIC;
	if (((current->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0 && (size_t*)&current->prev_footer > (size_t*)sf_mem_start()) { // Coalesce with previous
		size_t size = (current->prev_footer ^ MAGIC) & ~7; // Previous block size
		struct sf_block* previous = (struct sf_block*) ((size_t*)current - (size / sizeof(size_t)));
		if (previous->body.links.next != NULL && previous->body.links.prev != NULL) {
			remove_freelist_block(previous);
		}
		size += (current->header ^ MAGIC) & ~7; // New size
		previous->header = size | ((previous->header ^ MAGIC) & PREV_BLOCK_ALLOCATED);
		previous->header ^= MAGIC;
		struct sf_block* previous_footer = (struct sf_block*) ((size_t*)previous + (size / sizeof(size_t)));
		previous_footer->header ^= MAGIC;
		previous_footer->header = previous_footer->header & ~PREV_BLOCK_ALLOCATED;
		previous_footer->header ^= MAGIC;
		previous_footer->prev_footer = previous->header;
		current = previous;
	}
	current_footer = (struct sf_block*) ((size_t*)current + ((current->header ^ MAGIC) & ~7) / sizeof(size_t));
	if (((current_footer->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == 0 && (size_t*)&current_footer->header < (size_t*)sf_mem_end()) {
		if (current_footer->body.links.next != NULL && current_footer->body.links.prev != NULL) {
			remove_freelist_block(current_footer);
		}
		size_t size = ((current_footer->header ^ MAGIC) & ~7) + ((current->header ^ MAGIC) & ~7);
		// struct sf_block* current_footer_footer = (struct sf_block*) ((size_t*)current_footer + size / sizeof(size_t));
		// current_footer_footer->prev_footer = current_footer->header;
		current->header = size | ((current->header ^ MAGIC) & PREV_BLOCK_ALLOCATED);
		current->header ^= MAGIC;
		struct sf_block* next = (struct sf_block*) ((size_t*)current + (size / sizeof(size_t)));
		next->prev_footer = current->header;
		next->header ^= MAGIC;
		next->header = next->header & ~PREV_BLOCK_ALLOCATED; // turn off previously allocated block;
		next->header ^= MAGIC;
	}
	int index = find_freelist_start((current->header ^ MAGIC) & ~7);
	// Insert
   	current->body.links.next = sf_free_list_heads[index].body.links.next;
	sf_free_list_heads[index].body.links.next = current;
	current->body.links.prev = &sf_free_list_heads[index];
   	current->body.links.next->body.links.prev = current;
}

void sf_free(void *pp) {
	if (pp == NULL) {
		abort();
	}
	if (((size_t)pp) % 16 != 0) {
		abort();
	}
	struct sf_block *current = (struct sf_block*) ((size_t*)pp - 16 / sizeof(size_t));
	size_t block_size = (current->header ^ MAGIC) & ~7;
	if (block_size < 32 || block_size % 16 != 0) {
		abort();
	}
	struct sf_block *current_footer = (struct sf_block*) ((size_t*)current + block_size / sizeof(size_t));
	if ((size_t*)current < (size_t*)sf_mem_start() || (size_t*)&current_footer->header > (size_t*)sf_mem_end()) {
		abort();
	}
	if (((current->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0 && ((current->prev_footer ^ MAGIC) & THIS_BLOCK_ALLOCATED) != 0 && (size_t*)&current->prev_footer != (size_t*)sf_mem_start()) {
		abort();
	}
	int index_quicklists = (block_size - 32) / 16;
	if (index_quicklists >= 0 && index_quicklists < NUM_QUICK_LISTS) {
		if (sf_quick_lists[index_quicklists].length < QUICK_LIST_MAX) {
			current->body.links.next = sf_quick_lists[index_quicklists].first;
			sf_quick_lists[index_quicklists].first = current;
			sf_quick_lists[index_quicklists].length++;
		} else {
			// TODO: Flush
			for (int i = 0; i < QUICK_LIST_MAX; i++) {
				struct sf_block *block_quicklists = sf_quick_lists[index_quicklists].first;
				sf_quick_lists[index_quicklists].first = block_quicklists->body.links.next;
				block_quicklists->body.links.next = NULL;
				block_quicklists->body.links.prev = NULL;
				coalesce_insert_freelist(block_quicklists);
			}
			current->body.links.next = NULL;
			sf_quick_lists[index_quicklists].first = current;
			sf_quick_lists[index_quicklists].length = 1;
		}
	} else {
		//printf("%s\n", "okok");
		coalesce_insert_freelist(current);
	}
    return;
}

/* resize current block to a new size "rsize"*/
void realloc_split(struct sf_block* current, size_t rsize) {
	size_t current_size = current->header ^ MAGIC;
	current->header = rsize | (current_size & PREV_BLOCK_ALLOCATED);
	current->header |= THIS_BLOCK_ALLOCATED;
	current->header ^= MAGIC;
	struct sf_block* remainder = (struct sf_block*) ((size_t*)current + rsize / sizeof(size_t));
	remainder->prev_footer = current->header;
	remainder->header = (current_size & ~7) - rsize;
	struct sf_block* remainder_footer = (struct sf_block*) ((size_t*)remainder + remainder->header / sizeof(size_t));
	remainder->header |= PREV_BLOCK_ALLOCATED;
	remainder->header ^= MAGIC;
	remainder_footer->prev_footer = remainder->header;
	remainder_footer->header ^= MAGIC;
	remainder_footer->header &= ~PREV_BLOCK_ALLOCATED;
	remainder_footer->header ^= MAGIC;
	if (((remainder_footer->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == 0) {
		struct sf_block* next = (struct sf_block*) ((size_t*)remainder_footer + ((remainder_footer->header ^ MAGIC) & ~7) / sizeof(size_t));
		next->prev_footer = remainder_footer->header;
	}
	coalesce_insert_freelist(remainder); //coalesce_insert_freelist(current);
}

void *sf_realloc(void *pp, size_t rsize) {
	if (pp == NULL) {
		abort();
	}
	if (((size_t)pp) % 16 != 0) {
		abort();
	}
	struct sf_block *current = (struct sf_block*) ((size_t*)pp - 16 / sizeof(size_t));
	size_t block_size = (current->header ^ MAGIC) & ~7;
	if (block_size < 32 || block_size % 16 != 0) {
		abort();
	}
	struct sf_block *current_footer = (struct sf_block*) ((size_t*)current + block_size / sizeof(size_t));
	if ((size_t*)&current->prev_footer < (size_t*)sf_mem_start() || (size_t*)&current_footer->header > (size_t*)sf_mem_end()) {
		abort();
	}
	if (((current->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0 && ((current->prev_footer ^ MAGIC) & THIS_BLOCK_ALLOCATED) != 0 && (size_t*)&current->prev_footer != (size_t*)sf_mem_start()) {
		abort();
	}
	if (rsize == 0) {
		sf_free(pp);
		return NULL;
	}
	if ((long)rsize < 0) {
		sf_errno = ENOMEM;
		sf_free(pp);
		return NULL;
	}
	size_t total_size = rsize + 8;
	if (total_size <= 32) {
		total_size = 32;
	} else if (total_size % 16 != 0) {
		total_size = (total_size / 16 + 1) * 16;
	}
	if (((current->header ^ MAGIC) & ~7) == total_size) { // Reallocating to the Same Size
		return pp;
	} else if (((current->header ^ MAGIC) & ~7) < total_size) { // Reallocating to a Larger Size
		void* new_payload = sf_malloc(rsize);
		if (new_payload == NULL) {
			return NULL;
		}
		memcpy(new_payload, pp, ((current->header ^ MAGIC) & ~7) - 8);
		sf_free(pp);
		return new_payload;
	} else { // Reallocating to a Samller Size
		if (((current->header ^ MAGIC) & ~7) - total_size < 32) {
			return pp;
		}
		realloc_split(current, total_size);
		//sf_show_heap();
		return pp;
	}
    return NULL;
}
