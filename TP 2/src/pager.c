/* UNIVERSIDADE FEDERAL DE MINAS GERAIS     *
 * DEPARTAMENTO DE CIENCIA DA COMPUTACAO    *
 * Copyright (c) Italo Fernando Scota Cunha */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include "mmu.h"
#include "pager.h"

#define INVALID_PID -1
typedef struct page_t
{
	int isvalid;
	int frame_number;
	int block_number;
	int used;
	intptr_t addr;
} page_t;

typedef struct page_table_t
{
	pid_t pid;
	page_t *pages;
	int page_count;
	int page_capacity;
} page_table_t;

typedef struct frames_t
{
	pid_t pid;
	int accessed;
	page_t *page;
} frames_t;

typedef struct frame_list_t
{
	int size;
	int page_size;
	int second_chance_index;
	frames_t *frames;
} frame_list_t;

typedef struct blocks_t
{
	int used;
	page_t *page;
} blocks_t;

typedef struct block_list_t
{
	int nblocks;
	blocks_t *blocks;
} block_list_t;

frame_list_t frame_list;
block_list_t block_list;
page_table_t *page_list;
int page_tables_count = 0;
int page_tables_capacity = 10;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Função auxiliar para liberar memória alocada para uma página
void free_page(page_t *page)
{
	if (page->isvalid == 1)
	{
		frame_list.frames[page->frame_number].pid = INVALID_PID;
	}
}

// Função auxiliar para liberar memória alocada para uma lista de páginas
void free_page_list(page_table_t *page_table_list)
{
	while (page_table_list->page_count > 0)
	{
		page_t *page = &page_table_list->pages[--page_table_list->page_count];
		block_list.blocks[page->block_number].page = NULL;
		free_page(page);
	}
	free(page_table_list->pages);
}

// Função auxiliar para obter um novo frame
int get_new_frame()
{
	for (int i = 0; i < frame_list.size; i++)
	{
		if (frame_list.frames[i].pid == INVALID_PID)
			return i;
	}
	return INVALID_PID;
}

// funçao auxiliar para obter um novo bloco
int get_new_block()
{
	for (int i = 0; i < block_list.nblocks; i++)
	{
		if (block_list.blocks[i].page == NULL)
			return i;
	}
	return INVALID_PID;
}

// Função auxiliar para encontrar uma tabela de páginas
page_table_t *find_page_table(pid_t pid)
{
	for (int i = 0; i < page_tables_count; i++)
	{
		if (page_list[i].pid == pid)
			return &page_list[i];
	}
	exit(INVALID_PID);
}

// Função auxiliar para encontrar uma página
page_t *get_page(page_table_t *page_table_list, intptr_t addr)
{
	for (int i = 0; i < page_table_list->page_count; i++)
	{
		if (addr >= page_table_list->pages[i].addr && addr < (page_table_list->pages[i].addr + frame_list.page_size))
			return &page_table_list->pages[i];
	}
	return NULL;
}

// Função do algoritmo de segunda chance
int second_chance()
{
	frames_t *frames = frame_list.frames;
	int frame_to_swap = INVALID_PID;

	while (frame_to_swap == INVALID_PID)
	{
		int index = frame_list.second_chance_index;
		if (frames[index].accessed == 0)
		{
			frame_to_swap = index;
		}
		else
		{
			frames[index].accessed = 0;
		}
		frame_list.second_chance_index = (index + 1) % frame_list.size;
	}

	return frame_to_swap;
}

// Função auxiliar para trocar uma página
void swap(int frame_no)
{
	if (frame_no == 0)
	{
		for (int i = 0; i < frame_list.size; i++)
		{
			page_t *page = frame_list.frames[i].page;
			mmu_chprot(frame_list.frames[i].pid, (void *)page->addr, PROT_NONE);
		}
	}

	frames_t *frame = &frame_list.frames[frame_no];
	page_t *removed_page = frame->page;
	removed_page->isvalid = 0;
	mmu_nonresident(frame->pid, (void *)removed_page->addr);

	if (removed_page->used == 1)
	{
		block_list.blocks[removed_page->block_number].used = 1;
		mmu_disk_write(frame_no, removed_page->block_number);
	}
}

// Função auxiliar para imprimir os bytes de uma página
void print_page_bytes(page_t *page, size_t len)
{
	char *buf = (char *)malloc(len + 1);

	for (size_t i = 0; i < len; i++)
	{
		buf[i] = pmem[page->frame_number * frame_list.page_size + i];
	}

	for (int i = 0; i < len; i++)
	{
		printf("%02x", (unsigned)buf[i]);
	}

	if (len > 0)
	{
		printf("\n");
	}

	free(buf);
}

// Função auxiliar para a lógica do pager_fault após verificar a validade da página
void handle_valid_page(page_t *page, pid_t pid, void *addr)
{
	mmu_chprot(pid, addr, PROT_READ | PROT_WRITE);
	frame_list.frames[page->frame_number].accessed = 1;
	page->used = 1;
}

// Função auxiliar para a lógica do pager_fault após verificar a invalidade da página
void handle_invalid_page(page_t *page, pid_t pid, void *addr)
{
	int frame_no = get_new_frame();

	if (frame_no == INVALID_PID)
	{
		frame_no = second_chance();
		swap(frame_no);
	}

	frames_t *frame = &frame_list.frames[frame_no];
	frame->pid = pid;
	frame->page = page;
	frame->accessed = 1;

	page->isvalid = 1;
	page->frame_number = frame_no;
	page->used = 0;

	if (block_list.blocks[page->block_number].used == 1)
	{
		mmu_disk_read(page->block_number, frame_no);
	}
	else
	{
		mmu_zero_fill(frame_no);
	}

	mmu_resident(pid, addr, frame_no, PROT_READ);
}

// Função auxiliar para verificar a validade da página antes de lidar com o pager_fault
void check_page_validity(pid_t pid, void *addr)
{
	page_table_t *page_table_list = find_page_table(pid);
	addr = (void *)((intptr_t)addr - (intptr_t)addr % frame_list.page_size);
	page_t *page = get_page(page_table_list, (intptr_t)addr);

	if (page->isvalid == 1)
	{
		handle_valid_page(page, pid, addr);
	}
	else
	{
		handle_invalid_page(page, pid, addr);
	}
}

/* `pager_init` is called by the memory management infrastructure to
 * initialize the pager.  `nframes` and `nblocks` are the number of
 * physical memory frames available and the number of blocks for
 * backing store, respectively. */
void pager_init(int nframes, int nblocks)
{
	pthread_mutex_lock(&mutex);
	frame_list.size = nframes;
	frame_list.page_size = sysconf(_SC_PAGESIZE);
	frame_list.second_chance_index = 0;

	frame_list.frames = malloc(nframes * sizeof(frames_t));
	for (int i = 0; i < nframes; i++)
	{
		frame_list.frames[i].pid = INVALID_PID;
	}

	block_list.nblocks = nblocks;
	block_list.blocks = malloc(nblocks * sizeof(blocks_t));
	for (int i = 0; i < nblocks; i++)
	{
		block_list.blocks[i].used = 0;
	}

	page_list = malloc(page_tables_capacity * sizeof(page_table_t));
	pthread_mutex_unlock(&mutex);
}

/* `pager_create` should initialize any resources the pager needs to
 * manage memory for a new process `pid`. */
void pager_create(pid_t pid)
{
	pthread_mutex_lock(&mutex);
	if (page_tables_count == page_tables_capacity)
	{
		page_tables_capacity *= 2;
		page_list = realloc(page_list, page_tables_capacity * sizeof(page_table_t));
	}

	page_table_t *page_table_list = &page_list[page_tables_count++];
	page_table_list->pid = pid;
	page_table_list->pages = malloc(0);
	page_table_list->page_count = 0;
	page_table_list->page_capacity = 0;

	pthread_mutex_unlock(&mutex);
}

/* `pager_extend` allocates a new page of memory to process `pid`
 * and returns a pointer to that memory in the process's address
 * space.  `pager_extend` need not zero memory or install mappings
 * in the infrastructure until the application actually accesses the
 * page (which will trigger a call to `pager_fault`).
 * `pager_extend` should return NULL is there are no disk blocks to
 * use as backing storage. */
void *pager_extend(pid_t pid)
{
	pthread_mutex_lock(&mutex);
	int block_no = get_new_block();

	if (block_no == INVALID_PID)
	{
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	page_table_t *page_table_list = find_page_table(pid);

	if (page_table_list->page_count == page_table_list->page_capacity)
	{
		page_table_list->page_capacity = page_table_list->page_capacity == 0 ? 1 : page_table_list->page_capacity * 2;
		page_table_list->pages = realloc(page_table_list->pages, page_table_list->page_capacity * sizeof(page_t));
	}

	page_t *page = &page_table_list->pages[page_table_list->page_count++];
	page->isvalid = 0;
	page->addr = UVM_BASEADDR + (page_table_list->page_count - 1) * frame_list.page_size;
	page->block_number = block_no;

	block_list.blocks[block_no].page = page;

	pthread_mutex_unlock(&mutex);
	return (void *)page->addr;
}

/* `pager_fault` is called when process `pid` receives
 * a segmentation fault at address `addr`.  `pager_fault` is only
 * called for addresses previously returned with `pager_extend`.  If
 * free memory frames exist, `pager_fault` should use the
 * lowest-numbered frame to service the page fault.  If no free
 * memory frames exist, `pager_fault` should use the second-chance
 * (also known as clock) algorithm to choose which frame to page to
 * disk.  Your second-chance algorithm should treat read and write
 * accesses the same (i.e., do not prioritize either).  As the
 * memory management infrastructure does not maintain page access
 * and writing information, your pager must track this information
 * to implement the second-chance algorithm. */
void pager_fault(pid_t pid, void *addr)
{
	pthread_mutex_lock(&mutex);
	check_page_validity(pid, addr);
	pthread_mutex_unlock(&mutex);
}

/* `pager_syslog prints a message made of `len` bytes following
 * `addr` in the address space of process `pid`.  `pager_syslog`
 * should behave as if making read accesses to the process's memory
 * (zeroing and swapping in pages from disk if necessary).  If the
 * processes tries to syslog a memory region it has not allocated,
 * then `pager_syslog` should return -1 and set errno to EINVAL; if
 * the syslog succeeds, it should return 0. */
int pager_syslog(pid_t pid, void *addr, size_t len)
{
	pthread_mutex_lock(&mutex);
	page_table_t *page_table_list = find_page_table(pid);
	char *buf = (char *)malloc(len + 1);
	page_t *page;

	for (size_t i = 0, m = 0; i < len; i++)
	{
		page = get_page(page_table_list, (intptr_t)addr + i);
		if (page == NULL)
		{
			free(buf);
			pthread_mutex_unlock(&mutex);
			return INVALID_PID;
		}
		buf[m++] = pmem[page->frame_number * frame_list.page_size + i];
	}

	print_page_bytes(page, len);

	free(buf);
	pthread_mutex_unlock(&mutex);
	return 0;
}

/* `pager_destroy` is called when the process is already dead.  It
 * should free all resources process `pid` allocated (memory frames
 * and disk blocks).  `pager_destroy` should not call any of the MMU
 * functions. */
void pager_destroy(pid_t pid)
{
	pthread_mutex_lock(&mutex);
	page_table_t *page_table_list = find_page_table(pid);
	free_page_list(page_table_list);
	pthread_mutex_unlock(&mutex);
}