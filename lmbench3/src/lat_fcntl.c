#include "bench.h"

/*
 * lat_pipe.c - pipe transaction test
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char	*id = "$Id: lat_pipe.c,v 1.8 1997/06/16 05:38:58 lm Exp $\n";

#include "bench.h"

struct	flock lock, unlock;
struct	flock s1, s2;

/*
 * Create two files, use them as a ping pong test.
 * Process A:
 *	lock(1)
 *	unlock(2)
 * Process B:
 *	unlock(1)
 *	lock(2)
 * Initial state:
 *	lock is locked
 *	lock2 is locked
 */

#define	waiton(fd)	fcntl(fd, F_SETLKW, &lock)
#define	release(fd)	fcntl(fd, F_SETLK, &unlock)

struct _state {
	char filename1[2048];
	char filename2[2048];
	int	pid;
	int	fd1;
	int	fd2;
};

void initialize(void* cookie);
void benchmark(uint64 iterations, void* cookie);
void cleanup(void* cookie);

void
procA(struct _state *state)
{
	if (waiton(state->fd1) == -1) {
		perror("lock of fd1 failed\n");
		cleanup(state);
		exit(1);
	}
	if (release(state->fd2) == -1) {
		perror("unlock of fd2 failed\n");
		cleanup(state);
		exit(1);
	}
	if (waiton(state->fd2) == -1) {
		perror("lock of fd2 failed\n");
		cleanup(state);
		exit(1);
	}
	if (release(state->fd1) == -1) {
		perror("unlock of fd1 failed\n");
		cleanup(state);
		exit(1);
	}
}

void
procB(struct _state *state)
{
	if (release(state->fd1) == -1) {
		perror("unlock of fd1 failed\n");
		cleanup(state);
		exit(1);
	}
	if (waiton(state->fd2) == -1) {
		perror("lock of fd2 failed\n");
		cleanup(state);
		exit(1);
	}
	if (release(state->fd2) == -1) {
		perror("unlock of fd2 failed\n");
		cleanup(state);
		exit(1);
	}
	if (waiton(state->fd1) == -1) {
		perror("lock of fd1 failed\n");
		cleanup(state);
		exit(1);
	}
}

void 
initialize(void* cookie)
{
	int	pid;
	char	buf[10000];
	struct _state* state = (struct _state*)cookie;

	sprintf(state->filename1, "/tmp/lmbench-fcntl%d.1", getpid());
	sprintf(state->filename2, "/tmp/lmbench-fcntl%d.2", getpid());

	unlink(state->filename1);
	unlink(state->filename2);
	if ((state->fd1 = open(state->filename1, O_CREAT|O_RDWR, 0666)) == -1) {
		perror("create");
		exit(1);
	}
	if ((state->fd2 = open(state->filename2, O_CREAT|O_RDWR, 0666)) == -1) {
		perror("create");
		exit(1);
	}
	write(state->fd1, buf, sizeof(buf));
	write(state->fd2, buf, sizeof(buf));
	lock.l_type = F_WRLCK;
	lock.l_whence = 0;
	lock.l_start = 0;
	lock.l_len = 1;
	unlock = lock;
	unlock.l_type = F_UNLCK;
	if (waiton(state->fd1) == -1) {
		perror("lock1");
		exit(1);
	}
	if (waiton(state->fd2) == -1) {
		perror("lock2");
		exit(1);
	}
	state->pid = getpid();
	switch (pid = fork()) {
	case -1:
		perror("fork");
		exit(1);
	case 0:
		for ( ;; ) {
			procB(state);
		}
		exit(0);
	default:
		state->pid = pid;
		break;
	}
}

void
benchmark(uint64 iterations, void* cookie)
{
	struct _state* state = (struct _state*)cookie;
	uint64	i;
	
	for (i = 0; i < iterations; ++i) {
		procA(state);
	}
}

void cleanup(void* cookie)
{
	int i;
	struct _state* state = (struct _state*)cookie;

	close(state->fd1);
	close(state->fd2);

	unlink(state->filename1);
	unlink(state->filename2);

	kill(state->pid, 9);
}

int
main(int ac, char **av)
{
	int	i;
	int	c;
	int	parallel = 1;
	struct _state state;
	char *usage = "[-P <parallelism>]\n";

	/*
	 * If they specified a parallelism level, get it.
	 */
	while (( c = getopt(ac, av, "P:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}

	benchmp(initialize, benchmark, cleanup, 0, parallel, &state);
	micro("Fcntl lock latency", get_n());

	return (0);
}