/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "exec_parser.h"

#define page_size getpagesize()
static so_exec_t *exec;
static int fd;


// verific daca maparea a fost facuta cu succes
static void check_mapping(void *map) {
	if (map == MAP_FAILED) {
		signal(SIGSEGV, SIG_DFL);
		raise(SIGSEGV);
	}
}

static void page_mapper(so_seg_t *segm, void *fault_addr)
{
	// voi folosi vectorul data din segment pentru a retine
	// paginile care deja au fost mapate

	if (segm->data == NULL) {
		int max_pages = segm->mem_size / page_size;

		segm->data = calloc(max_pages, sizeof(char));
	}

	// caut indexul si adresa paginii care trebuie mapata
	int page_nr = ((int) (fault_addr - segm->vaddr)) / page_size;
	void *page_addr = ((void *)segm->vaddr) + page_nr * page_size;

	// daca pagina a fost deja mapata, inseamna ca are loc
	// o eroare de permisiuni. folosesc handler-ul default
	if (((char *)segm->data)[page_nr] == -1) {
		signal(SIGSEGV, SIG_DFL);
		raise(SIGSEGV);
	}

	// maparea paginii
	void *mapped_page;
	int non_zero = 0;

	// daca pagina se afla undeva inainte de filesize, mapez folosind date din fisier
	if (page_nr * page_size < segm->file_size) {
		mapped_page = mmap(page_addr, page_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, segm->offset + page_nr * page_size);
		check_mapping(mapped_page);

		// daca incepe inaintea lui mem_size, pagina va contine in rest doar zerouri
		// altfel pun zerouri in spatiul dintre file_size si mem_size
		if (((page_nr + 1) * page_size - 1) > segm->file_size) {
			if (segm->file_size < segm->mem_size) {
				non_zero = segm->file_size - page_nr * page_size;
				if ((page_nr + 1) * page_size - 1 > segm->mem_size)
					memset((void *)(segm->vaddr + segm->file_size), 0, segm->mem_size - segm->file_size);
				else
					memset(page_addr + non_zero, 0, page_size - non_zero);
			}
		}
	} else {
		// pagina incepe dupa file_size, deci maparea poate fi anonima
		mapped_page = mmap(page_addr, page_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED | MAP_ANON, -1, 0);
		check_mapping(mapped_page);
		// verific daca pagina contine date cu comportament nedefinit
		// daca da, pun zerouri de la file_size pana la mem_size
		if ((page_nr + 1) * page_size - 1 > segm->mem_size)
			memset((void *)(segm->vaddr + segm->file_size), 0, segm->mem_size - segm->file_size);
	}

	// marchez pagina ca mapata
	((char *)segm->data)[page_nr] = -1;

	// mapez si permisiunile aferente
	if (mprotect(mapped_page, page_size, segm->perm) == -1)
		exit(EXIT_FAILURE);
}

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	if (info == NULL || info->si_addr == NULL)
		exit(EXIT_FAILURE);

	void *fault_addr = info->si_addr;
	void *seg_addr;
	int seg_size, i;

	// caut segmentul care contine adresa care a cauzat fault-ul
	for (i = 0 ; i < exec->segments_no; i++) {
		seg_addr = (void *)(((exec->segments) + i)->vaddr);
		seg_size = ((exec->segments) + i)->mem_size;
		if (fault_addr >= seg_addr && fault_addr < seg_addr + seg_size)
			break;
	}

	//daca n-am gasit niciun segment potrivit, chiar este un segfault
	//las handler-ul default sa se ocupe de el
	if (i == exec->segments_no) {
		signal(SIGSEGV, SIG_DFL);
		raise(SIGSEGV);
	} else
		page_mapper((exec->segments) + i, fault_addr);
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;
	fd = open(path, O_RDWR);
	so_start_exec(exec, argv);
	close(fd);
	return -1;
}
