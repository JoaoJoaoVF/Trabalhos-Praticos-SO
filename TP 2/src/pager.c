/* UNIVERSIDADE FEDERAL DE MINAS GERAIS     *
 * DEPARTAMENTO DE CIENCIA DA COMPUTACAO    *
 * Copyright (c) Italo Fernando Scota Cunha */

#include <sys/types.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "mmu.h"
#include "pager.h"

#include <stdlib.h>
#include <assert.h>

#define NON_DEFINED -10;

// estrutura de um quadro na memória
typedef struct frame
{
	pid_t pid;
	intptr_t addr;
	int free;
	int second_chance;
} frame;

// estrutura da tabela de páginas de um processo
typedef struct page_table
{
	int *frames;
	int *blocks;
} page_table;

// estrutura da tabela geral de páginas
typedef struct page_table_list
{
	pid_t pid;
	page_table *pages;
} page_table_list;

frame *frames_list;
int max_nframes = 0;

int *blocks_list;
int max_nblocks = 0;
int blocks_size = 0;

page_table_list *pt_list;
int pt_list_size = 1;

int clock_index = 0;
intptr_t addr = UVM_BASEADDR;
int num_pages;
pthread_mutex_t mutex;

/* `pager_init` is called by the memory management infrastructure to
 * initialize the pager.  `nframes` and `nblocks` are the number of
 * physical memory frames available and the number of blocks for
 * backing store, respectively. */
void pager_init(int nframes, int nblocks)
{

	pthread_mutex_lock(&mutex);
	int i = 0;

	num_pages = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

	pt_list = (page_table_list *)malloc(1 * sizeof(page_table_list));
	pt_list[0].pid = NON_DEFINED;
	pt_list[0].pages = NULL;

	frames_list = (frame *)malloc(nframes * sizeof(frame));
	max_nframes = nframes;

	blocks_list = (int *)malloc(nblocks * sizeof(int));
	max_nblocks = nblocks;

	for (i = 0; i < max_nblocks; i++)
		blocks_list[i] = 0;

	for (i = 0; i < max_nframes; i++, addr += sysconf(_SC_PAGESIZE))
	{
		frames_list[i].pid = NON_DEFINED;
		frames_list[i].addr = addr;
		frames_list[i].free = 1;
		frames_list[i].second_chance = 0;
	}

	pthread_mutex_unlock(&mutex);
}

/* `pager_create` should initialize any resources the pager needs to
 * manage memory for a new process `pid`. */

void pager_create(pid_t pid)
{
	pthread_mutex_lock(&mutex);
	int i = 0, j = 0, found_page = 0;
	for (i = 0; i < pt_list_size; i++)
	{
		if (pt_list[i].pages == NULL)
		{
			pt_list[i].pid = pid;
			pt_list[i].pages = (page_table *)malloc(sizeof(page_table));
			pt_list[i].pages->frames = (int *)malloc(num_pages * (sizeof(int)));
			pt_list[i].pages->blocks = (int *)malloc(num_pages * (sizeof(int)));
		}

		for (j = 0; j < num_pages; j++)
		{
			pt_list[i].pages->frames[j] = NON_DEFINED;
			pt_list[i].pages->blocks[j] = NON_DEFINED;
		}

		found_page = 1;
	}

	if (!found_page)
	{
		pt_list = realloc(pt_list, (100 + pt_list_size) * sizeof(pt_list));

		pt_list[pt_list_size].pid = pid;
		pt_list[pt_list_size].pages = (page_table *)malloc(sizeof(page_table));
		pt_list[pt_list_size].pages->frames = (int *)malloc(num_pages * sizeof(int));
		pt_list[pt_list_size].pages->blocks = (int *)malloc(num_pages * sizeof(int));

		for (j = 0; j < num_pages; j++)
		{
			pt_list[pt_list_size].pages->frames[j] = NON_DEFINED;
			pt_list[pt_list_size].pages->blocks[j] = NON_DEFINED;
		}

		int pt_list_index = pt_list_size;
		pt_list_size += 100;

		for (j = pt_list_index; j < pt_list_size; j++)
			pt_list[j].pages = NULL;
	}

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

	if (blocks_size == max_nblocks)
	{
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	int block_found, return_index = 0, i = 0, block_index = 0;

	for (i = 0; i < max_nblocks; i++)
	{
		if (blocks_list[i] == 0)
		{
			blocks_list[i] = 1;
			block_found = i;
			blocks_size++;
			break;
		}
	}

	for (i = 0; i < pt_list_size; i++)
	{

		if (pt_list[i].pid == pid)
		{

			for (block_index = 0; block_index < num_pages; block_index++)
			{

				if (pt_list[i].pages->blocks[block_index] == -10)
				{

					pt_list[i].pages->blocks[block_index] = block_found;
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&mutex);
	return (void *)(UVM_BASEADDR + (intptr_t)(block_index * sysconf(_SC_PAGESIZE)));
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
	int i, index;

	for (i = 0; i < pt_list_size; i++)
	{
		if (pt_list[i].pid == pid)
		{
			index = i;
			break;
		}
	}

	int page_num = ((((intptr_t)addr) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)));
	int current_frame, temp_frame, temp_disk_block;

	if (pt_list[index].pages->frames[page_num] != -10 &&
		pt_list[index].pages->frames[page_num] != -2) // NON_DEFINED nao deu pra usar
	{
		current_frame = pt_list[index].pages->frames[page_num];
		mmu_chprot(pid, addr, PROT_READ | PROT_WRITE);
	}
	else
	{
		temp_frame = -1;

		for (i = 0; i < max_nframes; i++)
		{
			if (frames_list[i].free)
			{
				temp_frame = i;
				frames_list[i].pid = pid;
				frames_list[i].addr = (intptr_t)addr;
				frames_list[i].free = 0;

				if (pt_list[index].pages->frames[page_num] == -2)
				{
					temp_disk_block = pt_list[index].pages->blocks[page_num];
					mmu_disk_read(temp_disk_block, temp_frame);
				}
				else
				{
					mmu_zero_fill(temp_frame);
				}
				pt_list[index].pages->frames[page_num] = temp_frame;
				mmu_resident(pid, addr, temp_frame, PROT_READ);
				break;
			}
		}

		if (temp_frame == -1)
		{
			temp_frame = 0;
			current_frame = pt_list[index].pages->frames[page_num];
			frames_list[temp_frame].pid = pid;
			frames_list[temp_frame].addr = (intptr_t)addr;

			if (pt_list[index].pages->frames[page_num] == -2)
			{
				temp_disk_block = pt_list[index].pages->blocks[page_num];
				mmu_disk_read(temp_disk_block, temp_frame);
			}
			else
			{
				mmu_zero_fill(temp_frame);
			}
			pt_list[index].pages->frames[page_num] = temp_frame;
			mmu_resident(pid, addr, temp_frame, PROT_READ);
			mmu_chprot(pid, addr, PROT_READ | PROT_WRITE);

			frames_list[current_frame].free = 1;
		}
	}
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

	for (int i = 0; i < pt_list_size; i++)
	{
		if (pt_list[i].pid == pid)
		{
			int curr_page = ((intptr_t)addr - UVM_BASEADDR) / sysconf(_SC_PAGESIZE);
			if (pt_list[i].pages->frames[curr_page] == -10)
			{
				pthread_mutex_unlock(&mutex);
				return -1;
			}
			break;
		}
	}

	char *buf = (char *)malloc(len * sizeof(char));

	memcpy(buf, addr, len);

	for (int i = 0; i < len; i++)
	{
		printf("%02x", (unsigned)buf[i]);
	}

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
	int i = 0;
	for (i = 0; i < pt_list_size; i++)
	{
		if (pt_list[i].pid == pid)
		{
			free(pt_list[i].pages->frames);
			free(pt_list[i].pages->blocks);
			free(pt_list[i].pages);

			pt_list[i].pid = NON_DEFINED;
			pt_list[i].pages = NULL;

			break;
		}
	}

	pthread_mutex_unlock(&mutex);
}
