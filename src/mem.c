
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
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
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
		if(index == seg_table->table[i].v_index){
			return seg_table->table[i].pages;
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
			*physical_addr=(page_table->table[i].p_index)*PAGE_SIZE+offset;
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

	uint32_t num_pages = (size % PAGE_SIZE) ? (size / PAGE_SIZE+1) : (size / PAGE_SIZE ); // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */
	int num_pages_empty=0;
	for(int i = 0; i<NUM_PAGES;i++){
		if(_mem_stat[i].proc == 0){
			num_pages_empty ++;
			if(num_pages_empty > num_pages)
				break;
		}
	}
	if (num_pages_empty > num_pages && (proc->bp +num_pages * PAGE_SIZE) <= RAM_SIZE) 
		mem_avail=1;
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
		int pages_index=0;
		int * mem_next;
		addr_t first_lv = get_first_lv(ret_mem);
		addr_t second_lv = get_second_lv(ret_mem);
		for(int i = 0 ; i< NUM_PAGES;i++){
			if(_mem_stat[i].proc == 0){
				
				/*Add entries*/

				int *seg_size= &(proc->seg_table->size);
				if(*seg_size == 0){
					(*seg_size)++;
					proc->seg_table->table[0].v_index=(addr_t)0;
					proc->seg_table->table[*seg_size-1].pages = (struct page_table_t *) malloc(sizeof(struct page_table_t));
					proc->seg_table->table[0].pages->size = 1;
					proc->seg_table->table[0].pages->table[0].v_index=(addr_t)0;
				}
				int *page_size = &(proc->seg_table->table[(*seg_size)-1].pages->size);

				if(*page_size == 32){
					(*seg_size)++;
					proc->seg_table->table[(*seg_size)-1].v_index=first_lv;
					proc->seg_table->table[(*seg_size)-1].pages = (struct page_table_t *) malloc(sizeof(struct page_table_t));
					struct page_table_t * new_page_table = proc->seg_table->table[*seg_size-1].pages;
					new_page_table->size++;
					new_page_table->table[0].v_index=second_lv;
					new_page_table->table[0].p_index=(addr_t)i;
					second_lv++;
				}else{
					(*page_size)++;
					struct page_table_t * old_page_table = proc->seg_table->table[*seg_size-1].pages;
					old_page_table->table[*page_size-1].p_index=(addr_t)i;
					old_page_table->table[*page_size-1].v_index=second_lv;
					if((*page_size) == 32){
						first_lv++;
						second_lv=0;
					}else
						second_lv++;
				}

				/*Update _mem_stat of emty pages*/
				if(pages_index != 0){
					*mem_next = i;
				}
				_mem_stat[i].proc = proc->pid;
				_mem_stat[i].index = pages_index;
				mem_next = &(_mem_stat[i].next);
				pages_index++;
				if(pages_index == num_pages) {
					_mem_stat[i].next=-1;
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

	addr_t first_lv=get_first_lv(address);
	addr_t second_lv=get_second_lv(address);
	addr_t physical_addr;
	int mem_index;
	int num_pages=0;
	int seg_index;
	int page_index;
	//struct page_table_t *page_table = get_page_table(first_lv,proc->seg_table);
	if(translate(address,&physical_addr,proc) ){
		mem_index = physical_addr >> OFFSET_LEN;
	}else {
		pthread_mutex_unlock(&mem_lock);
		return 1;
	}

	do{
		num_pages++;
		_mem_stat[mem_index].proc=0;
		mem_index=_mem_stat[mem_index].next;
	}while(mem_index != -1);

	for(int i =0 ; i< proc->seg_table->size; i++){
		if(proc->seg_table->table[i].v_index == first_lv){
			seg_index = i ;
			for(int j = 0;j < proc->seg_table->table[i].pages->size; j++){
				if(proc->seg_table->table[i].pages->table[j].v_index == second_lv){
					page_index = j;
					break;
				}
			}
			break;

		}
	}
	int *seg_size = &(proc->seg_table->size);
	for(int i = 0 ; i < num_pages ;i++){
		for(int m = seg_index;m < (*seg_size);m++){
			int *page_size = &(proc->seg_table->table[m].pages->size);
			int page_idx;
			if(m == seg_index)
				page_idx=page_index;
			else
				page_idx =0;
			for(int n = page_idx;n<(*page_size);n++){
				if(n<(*page_size)-1){
					proc->seg_table->table[m].pages->table[n]=proc->seg_table->table[m].pages->table[n+1];
				}else if( m < (*seg_size)-1){
					proc->seg_table->table[m].pages->table[n]=proc->seg_table->table[m+1].pages->table[0];
				}
			}
			if(m == (*seg_size)-1){
				(*page_size)--;
				if((*page_size)==0){
					(*seg_size)--;
					free(proc->seg_table->table[m].pages);
				}
			}
		}
	}


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


