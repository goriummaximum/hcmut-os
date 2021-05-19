
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


/* Segmentation with paging mechanism: 
	1. Break the address into 3 parts: Address = Segment + Index + Offset
	2. Use segment index to find the page table entry No. x (x = 1,2,3,...)
	2. Obtain a page table from the segment, use page index to find the frame containing index of physical address
	3. Get the index of physical address in page table, concatenate with the offset: | p_index | offset |
	4. The result | index | offset | is the translated physical address that will mapped into the physical memory
*/

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN); // Offset <-> 10 bits
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN); // Segment index <-> 5 bits
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN); // Page index <-> 5 bits
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
		if (seg_table->table[i].v_index == index) { 
			return seg_table->table[i].pages; // Return the page table from the segment index
		}
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
	addr_t offset = get_offset(virtual_addr); // Offset <-> 10 bits
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr); // Segment index <-> 5 bits
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr); // Page index <-> 5 bits
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table); 
	/*^ By using the first_lvl which is the segment index, we can find the page table
		corresponding to it and then use the page table for the later process*/
	if (page_table == NULL) {
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++) {
		if (page_table->table[i].v_index == second_lv) {

			/* The second_lvl is the page index, that will used along with page base number
			   to find the correct frame number in page table of the corresponding segment */

			/* In short: It it used to find the frame number in page table */

			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */

			addr_t physical_index = page_table->table[i].p_index;
			* physical_addr = (physical_index << OFFSET_LEN) | offset; // Concatenate and save to p_addr
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
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	// Check free page (page with proc == 0) in physical memory
	int count_page = 0;
	for (int i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc == 0) {
			count_page+=1;
			if (count_page == num_pages) {
				break;
			}
		}
	}

	if (count_page < num_pages) mem_avail = 0; // Not enough space to allocate new memory region
	
	// Check free space in virtual memory
	else if (proc->bp + num_pages*PAGE_SIZE >= RAM_SIZE) mem_avail = 0; // Not enough RAM for allocating

	else mem_avail = 1;

	
	if (mem_avail) {
		/* We could allocate new memory region to the process */

		ret_mem = proc->bp; // Save the first byte of page to ret_mem
		proc->bp += num_pages * PAGE_SIZE;

		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table and page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */

		int alloc_count_page = 0;
		int last_page = -1; // The tail of page list
		for (int i = 0; i < NUM_PAGES; i++) {
			// There is value in proc, which means the page is used => SKIP
			if (_mem_stat[i].proc) continue;

			// Otherwise, update the proc, index and next field
			_mem_stat[i].proc = proc->pid; // Update proc
			_mem_stat[i].index = alloc_count_page; // Update index

			if (last_page > -1) {
				// Condition when the current page is not the initial page
				// (because after initial page, [last_page] value will be different from -1)

				_mem_stat[last_page].next = i; // Update the [next] field of the previous page
			}
			last_page = i; // Save the index of the previous page

			// Add entries to segment table(1) and page table(2) (to be updated)

			// Find virual page table from segment
			addr_t v_address = ret_mem + alloc_count_page * PAGE_SIZE; // Virtual address 
			addr_t v_segment = get_first_lv(v_address); // Get the segment index (5 bits)
			struct page_table_t * virtual_page_table = NULL; // Initialize new temporary page table
			virtual_page_table = get_page_table(v_segment, proc->seg_table); // Get the virtual page table entries from segment
			

			// Note: If there is no virtual page table found from above command
			//  -> Create a new page table in the segment table


			alloc_count_page+=1;

			if (alloc_count_page == num_pages) { // Allocation reach its maximum
				_mem_stat[i].next = -1;
				break;
			}
		}
		
	}
	pthread_mutex_unlock(&mem_lock);
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
	
	//Free Physical mem
	addr_t p_index = 0 >> OFFSET_LEN;
	while(_mem_stat[p_index].next != -1) //Iterate through physical memory
	{
		_mem_stat[p_index].proc = 0; // set the [proc] value in _mem_stat to 0 to indicate that it is free
		p_index = _mem_stat[p_index].next; 
	}

	//Free Virtual mem
	



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


