
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
} _mem_stat [NUM_PAGES]; //check status of physical page

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

			/* In short: It it used to find the frame number in page table

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

	uint32_t num_pages = (size % PAGE_SIZE) ? size / PAGE_SIZE + 1 :
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
	//Check physical
	int phy_free_pages = 0;
	for (int i = 0; i < NUM_PAGES; i++)
	{
		if (_mem_stat[i].proc == 0) phy_free_pages++;
	}

	mem_avail = phy_free_pages >= num_pages;

	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */

		int curr_page = 0;
		int prev_mem_index = -1;
		for (int i = 0; i < NUM_PAGES; i++)
		{
			if (_mem_stat[i].proc == 0)
			{
				//Update [proc], [index], and [next] field
				_mem_stat[i].proc = proc->pid;
				_mem_stat[i].index = curr_page;
				if (prev_mem_index != -1)
					_mem_stat[prev_mem_index].next = i;
				prev_mem_index = i;

				//Add entries to segment table page tables of [proc]
				addr_t cur_vir_addr = ret_mem + curr_page * PAGE_SIZE;
				struct page_table_t *located_page_table = get_page_table(get_first_lv(cur_vir_addr), proc->seg_table);

				if (located_page_table == NULL)
				{
					struct seg_table_t *located_seg_table = proc->seg_table;
					int first_lv_last = located_seg_table->size;
					located_seg_table->table[first_lv_last].v_index = get_first_lv(cur_vir_addr);
					located_seg_table->table[first_lv_last].pages = (struct page_table_t *)malloc(sizeof(struct page_table_t));
					located_seg_table->size++;

					int second_lv_last = located_seg_table->table[first_lv_last].pages->size;
					located_seg_table->table[first_lv_last].pages->table[second_lv_last].v_index = get_second_lv(cur_vir_addr);
					located_seg_table->table[first_lv_last].pages->table[second_lv_last].p_index = i;
					located_seg_table->table[first_lv_last].pages->size++;
					//printf("alloc 	first lv: %x	second lv: %x 	curr_page: %d\n", proc->seg_table->table[first_lv_last].v_index, proc->seg_table->table[first_lv_last].pages->table[second_lv_last].v_index, curr_page);
				}

				else
				{
					located_page_table->table[located_page_table->size].v_index = get_second_lv(cur_vir_addr);
					located_page_table->table[located_page_table->size].p_index = i;
					located_page_table->size++;
					//printf("alloc 	first lv: %x	second lv: %x	curr_page: %d\n", proc->seg_table->table[0].v_index, proc->seg_table->table[0].pages->table[proc->seg_table->table[0].pages->size - 1].v_index, curr_page);
				}

				curr_page++;
				if (curr_page == num_pages)
				{
					_mem_stat[i].next = -1;
					break;
				}
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

	addr_t phy_addr;
	if (translate(address, &phy_addr, proc) == 0)
	{
		pthread_mutex_unlock(&mem_lock);
		return 0;
	}
	int num_pages = 0;

	for (int i = get_second_lv(phy_addr); i != -1; i = _mem_stat[i].next)
	{
		_mem_stat[i].proc = 0;
		num_pages++;
	}
	
	proc->bp = proc->bp - num_pages * PAGE_SIZE;

	int curr_page = 0;
	while (curr_page < num_pages)
	{
		int vir_second_lv = get_second_lv(address);
		int vir_first_lv = get_first_lv(address);
		//printf("free 	first lv: %x	second lv: %x	curr_page: %d\n", vir_first_lv, vir_second_lv, curr_page);
		struct page_table_t *page_table = get_page_table(vir_first_lv, proc->seg_table);
		for (int i = 0; i < page_table->size; i++)
		{
			if (vir_second_lv == page_table->table[i].v_index)
			{
				for (int j = i; j < page_table->size - 1; j++)
				{
					page_table->table[j].p_index = page_table->table[j + 1].p_index;
					page_table->table[j].v_index = page_table->table[j + 1].v_index;
				}

				page_table->size--;

				if (page_table->size == 0)
				{
					free(page_table);
					for (int j = vir_first_lv; j < proc->seg_table->size - 1; j++)
					{
						proc->seg_table->table[j].pages = proc->seg_table->table[j + 1].pages;
						proc->seg_table->table[j].v_index = proc->seg_table->table[j + 1].v_index;
					}
					proc->seg_table->table[proc->seg_table->size - 1].pages = NULL;
					proc->seg_table->size--;
				}
				i--;
			}
		}
		curr_page++;
		address = address + PAGE_SIZE;
	}

	pthread_mutex_unlock(&mem_lock);

	return 0;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		pthread_mutex_lock(&mem_lock);
		*data = _ram[physical_addr];
		pthread_mutex_unlock(&mem_lock);
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		pthread_mutex_lock(&mem_lock);
		_ram[physical_addr] = data;
		pthread_mutex_unlock(&mem_lock);
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
