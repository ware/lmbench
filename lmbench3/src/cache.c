/*
 * tlb.c - guess the cache line size
 *
 * usage: tlb
 *
 * Copyright (c) 2000 Carl Staelin.
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id$\n";

#include "bench.h"

struct _state {
	char*	addr;
	char*	p;
	int	len;
	int	line;
	int	pagesize;
};

void compute_times(struct _state* state, double* tlb_time, double* cache_time);
void initialize(void* cookie);
void benchmark(uint64 iterations, void* cookie);
void cleanup(void* cookie);

#define	ONE	p = (char **)*p;
#define	FIVE	ONE ONE ONE ONE ONE
#define	TEN	FIVE FIVE
#define	FIFTY	TEN TEN TEN TEN TEN
#define	HUNDRED	FIFTY FIFTY

/*
 * Assumptions:
 *
 * 1) Cache lines are a multiple of pointer-size words
 * 2) Cache lines are smaller than 1/4 a page size
 * 3) Pages are an even multiple of cache lines
 */
int
main(int ac, char **av)
{
	int	i, l, len;
	int	c;
	int	print_cost = 0;
	int	maxlen = 32 * 1024 * 1024;
	double	time;
	struct _state state;
	char   *usage = "[-c] [-L <line size>] [-M len[K|M]]\n";

	state.line = getpagesize() / (4 * sizeof(char*));
	state.pagesize = getpagesize();

	while (( c = getopt(ac, av, "cL:M:")) != EOF) {
		switch(c) {
		case 'c':
			print_cost = 1;
			break;
		case 'L':
			state.line = atoi(optarg);
			break;
		case 'M':
			l = strlen(optarg);
			if (optarg[l-1] == 'm' || optarg[l-1] == 'M') {
				maxlen = 1024 * 1024;
				optarg[l-1] = 0;
			} else if (optarg[l-1] == 'k' || optarg[l-1] == 'K') {
				maxlen = 1024;
				optarg[l-1] = 0;
			}
			maxlen *= atoi(optarg);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	for (i = sizeof(char*); i <= maxlen; i<<=1) {
		state.len = i;
		benchmp(initialize, benchmark, cleanup, 0, 1, &state);

		/* We want nanoseconds / load. */
		time = (1000. * (double)gettime()) / (100. * (double)get_n());
		fprintf(stderr, "cache: %d bytes %.5f nanoseconds\n", state.len, time);
	}

	if (print_cost) {
		fprintf(stderr, "cache: %d bytes %.5f nanoseconds\n", len, time);
	} else {
		fprintf(stderr, "cache: %d bytes\n", len);
	}

	return(0);
}


/*
 * This will access len bytes
 */
void
initialize(void* cookie)
{
	int i, j, k, nwords, nlines, nbytes, npages, npointers;
	unsigned int r;
	char ***pages;
	int    *lines;
	int    *words;
	struct _state* state = (struct _state*)cookie;
	register char *p = 0 /* lint */;

	nbytes = state->len;
	npointers = state->len / sizeof(char*);
	nwords = state->line / sizeof(char*);
	nlines = state->pagesize / state->line;
	npages = (nbytes + state->pagesize) / state->pagesize;

	words = (int*)malloc(nwords * sizeof(int));
	lines = (int*)malloc(nlines * sizeof(int));
	pages = (char***)malloc(npages * sizeof(char**));
	state->p = state->addr = (char*)malloc(nbytes + 2 * state->pagesize);

	if ((unsigned long)state->p % state->pagesize) {
		state->p += state->pagesize - (unsigned long)state->p % state->pagesize;
	}

	if (state->addr == NULL || pages == NULL) {
		exit(0);
	}

	srand(getpid());

	/* first, layout the sequence of page accesses */
	p = state->p;
	for (i = 0; i < npages; ++i) {
		pages[i] = (char**)p;
		p += state->pagesize;
	}

	/* randomize the page sequences (except for zeroth page) */
	r = (rand() << 15) ^ rand();
	for (i = npages - 2; i > 0; --i) {
		char** l;
		r = (r << 1) ^ (rand() >> 4);
		l = pages[(r % i) + 1];
		pages[(r % i) + 1] = pages[i + 1];
		pages[i + 1] = l;
	}

	/* layout the sequence of line accesses */
	for (i = 0; i < nlines; ++i) {
		lines[i] = i * state->pagesize / (nlines * sizeof(char*));
	}

	/* randomize the line sequences */
	for (i = nlines - 2; i > 0; --i) {
		int l;
		r = (r << 1) ^ (rand() >> 4);
		l = lines[(r % i) + 1];
		lines[(r % i) + 1] = lines[i];
		lines[i] = l;
	}

	/* layout the sequence of word accesses */
	for (i = 0; i < nwords; ++i) {
		words[i] = i * state->line / (nwords * sizeof(char*));
	}

	/* randomize the line sequences */
	for (i = nwords - 2; i > 0; --i) {
		int l;
		r = (r << 1) ^ (rand() >> 4);
		l = words[(r % i) + 1];
		words[(r % i) + 1] = words[i];
		words[i] = l;
	}

	/* setup the run through the pages */
	for (i = 0, k = 0; i < npages; ++i) {
		for (j = 0; j < nlines - 1 && k < npointers - 1; ++j) {
			pages[i][lines[j]+words[k%nwords]] = (char*)(pages[i] + lines[j+1] + words[(k+1)%nwords]);
			k++;
		}
		if (i == npages - 1 || k == npointers - 1) {
			pages[i][lines[j]+words[k%nwords]] = (char*)(pages[0] + lines[0] + words[0]);
		} else {
			pages[i][lines[j]+words[k%nwords]] = (char*)(pages[i+1] + lines[0] + words[(k+1)%nwords]);
		}
		k++;
	}

	free(pages);
	free(lines);
	free(words);

	/* run through the chain once to clear the cache */
	benchmark((npointers + 100) / 100, state);
}


void benchmark(uint64 iterations, void *cookie)
{
	struct _state* state = (struct _state*)cookie;
	static char **p_save = NULL;
	register char **p = p_save ? p_save : (char**)state->p;

	while (iterations-- > 0) {
		HUNDRED;
	}

	use_pointer((void *)p);
	p_save = p;
}

void cleanup(void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	free(state->addr);
	state->addr = NULL;
	state->p = NULL;
}


