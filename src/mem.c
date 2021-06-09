
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page
	int index;	// Index of the page in the list of pages allocated
			// to the process.
	int next;	// The next page in the list. -1 if it is the last
			// page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
	// 000000000-0000
	// 111111111-1111
	// 111111111-0000
	// 000000000-1111 & address = offset
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
	// 100-1001-01101
	// 00000-100-1001 - 00001000000 // AB-A0 = B

}

/* Search for page table table from the a segment table */
static struct page_table_t * get_page_table(
		addr_t index, 	// Segment level index
		struct seg_table_t * seg_table) { // first level table
	
	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;
	for (i = 0; i < seg_table->size; i++) {
		// Enter your code here
		if(seg_table->table[i].v_index == index)
			return seg_table->table[i].pages;
	}
	return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr, 	// Given virtual address
		addr_t * physical_addr, // Physical address to be returned
		struct pcb_t * proc) {  // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL) {
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++) {
		if (page_table->table[i].v_index == second_lv) {
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			addr_t p_idx = page_table->table[i].p_index;
			// Concate p_idx with offset
			*physical_addr = (p_idx << OFFSET_LEN) | offset;
			return 1;
		}
	}
	return 0;	
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = (size % PAGE_SIZE) ? size / PAGE_SIZE :
		size / PAGE_SIZE + 1; // Number of pages we will use
	
	num_pages = (size % PAGE_SIZE) ? size / PAGE_SIZE + 1:
		size / PAGE_SIZE; // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */
		
	// Check free amount for required memory
	int free_physical = 0;
	int i;
	for(i = 0; i < NUM_PAGES; i++){
		if(_mem_stat[i].proc == 0){
			free_physical++;
			if(free_physical == num_pages){
				mem_avail = 1;
				break;
			}
		}
	}
	if (proc->bp + num_pages * PAGE_SIZE >= RAM_SIZE)
		mem_avail = 0;

	if (mem_avail) {
		/* We could allocate new memory region to the process */
		// Virtual address is returned
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */

		int pre_frame = 0;
		int count_page = 0;
		for(i = 0; i < NUM_PAGES; i++){
			if(_mem_stat[i].proc == 0){ // Free
				_mem_stat[i].proc = proc->pid;
				_mem_stat[i].index = count_page;
				_mem_stat[i].next = -1; // Insert to the end?
				// First block
				if(count_page == 0);
				else
					_mem_stat[pre_frame].next = i;
				pre_frame = i;

				// Get segment index and page table index of proc
				addr_t v_addr = ret_mem + count_page*PAGE_SIZE; 
				addr_t seg_idx = get_first_lv(v_addr);
				addr_t page_idx = get_second_lv(v_addr);
				
				struct page_table_t * page_table = get_page_table(seg_idx, proc->seg_table);
				if(page_table != NULL){
					int page_size = proc->seg_table->table[seg_idx].pages->size++;
					page_table->table[page_size].v_index = page_idx;		
					page_table->table[page_size].p_index = i;
				}
				else {
					int segment_size = proc->seg_table->size++;
					proc->seg_table->table[segment_size].pages = 
						(struct page_table_t *)malloc(sizeof(struct page_table_t));
					proc->seg_table->table[segment_size].pages->size = 1;
					proc->seg_table->table[segment_size].v_index = seg_idx;
					proc->seg_table->table[segment_size].pages->table[0].v_index = page_idx;
					proc->seg_table->table[segment_size].pages->table[0].p_index = i;

				}
				if(++count_page == num_pages){
					break;
				}		
			}
		}
	}
	pthread_mutex_unlock(&mem_lock);
	// printf("======== ALLOC size = %5d, pid = %d =========\n",size,proc->pid);
	// dump();
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */
	pthread_mutex_lock(&mem_lock);
	addr_t physical_addr;
    if (translate(address, &physical_addr, proc) != 0){
		int count_page = 0;
		int next = physical_addr >> OFFSET_LEN;
		while (next != -1)
		{
			_mem_stat[next].proc = 0;
			next = _mem_stat[next].next;

			// Free seg table va page table
			addr_t v_addr = address + count_page*PAGE_SIZE; 
			addr_t seg_idx = get_first_lv(v_addr);
			addr_t page_idx = get_second_lv(v_addr);

			for (int i = 0; i < proc->seg_table->size; i++)
			{
				if (proc->seg_table->table[i].v_index != seg_idx)
					continue;

				struct page_table_t * page_table = proc->seg_table->table[i].pages;
				for (int m = 0; m < page_table->size; m++)
				{
					if (page_table->table[m].v_index == page_idx)
					{
						// Merge and delete last element
						int k = 0;
						for (k = m; k < page_table->size - 1; k++)
						{
							page_table->table[k].v_index = page_table->table[k + 1].v_index;
							page_table->table[k].p_index = page_table->table[k + 1].p_index;
						}
						page_table->table[k].v_index = 0;
						page_table->table[k].p_index = 0;
						page_table->size--;
						break;
					}
				}
				if (proc->seg_table->table[i].pages->size == 0)
				{
					// Free empty page table
					free(proc->seg_table->table[i].pages);
					int m = 0;
					for (m = i; m < proc->seg_table->size - 1; m++)
					{
						proc->seg_table->table[m].v_index = proc->seg_table->table[m + 1].v_index;
						proc->seg_table->table[m].pages = proc->seg_table->table[m + 1].pages;
					}
					proc->seg_table->table[m].v_index = 0;
					proc->seg_table->table[m].pages = NULL;
					proc->seg_table->size--;
				}
				break;
			}
			count_page++;
		}
	}
	
	// printf("=============== FREE pid = %d =================\n",proc->pid);
	// dump();
	pthread_mutex_unlock(&mem_lock);

	return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		*data = _ram[physical_addr];
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		_ram[physical_addr] = data;
		return 0;
	}else{
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {
				
				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
			}
		}
	}
}


