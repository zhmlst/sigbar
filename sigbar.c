#define _GNU_SOURCE
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <linux/memfd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

extern char **environ;

#define BUFFER 64
#define LENGTH(X) (sizeof(X) / sizeof(X[0]))

typedef struct {
	char buffer[BUFFER];
	pid_t pid;
	int sv;
} Proc;

typedef struct {
	const char *command;
	const unsigned int signal;
} Spec;

#include "config.h"

static void die_if(int condition, const char *errmsg);
static void handle_signal(int sig);
static void setup_signals(void);
static void excluding_puts(const char *str, const char *exc);
static void print_status(void);
static int update_buffer(Proc *p);
static void* wait_events(void *arg);
static void reg_proc(Proc *p, int epfd);
static void fd_set_nonblock(int fd);
static void run_command(const char *cmd, int sv);
static int make_proc(Proc *p);
static void exec_memfd(int fd);
static int make_runnable_memfd(const char *cmd);
static void run_all(int epfd);

static Proc procs[LENGTH(specs)];

void
die_if(int condition, const char *errmsg)
{
	if (condition) {
		perror(errmsg);
		exit(EXIT_FAILURE);
	}
}

void
handle_signal(int s)
{
	for (size_t i = 0; i < LENGTH(procs); i++) {
		if (specs[i].signal == s - SIGRTMIN) {
			ssize_t w = write(procs[i].sv, "\n", 1);
			die_if(w < 0, "write(proc.sv)");
		}
	}
}

void
setup_signals(void)
{
	struct sigaction sa = {0};
	sa.sa_flags = SA_RESTART | SA_NOCLDWAIT;
	sa.sa_handler = handle_signal;
	for (size_t i = 0; i < LENGTH(specs); i++)
		sigaction(SIGRTMIN + specs[i].signal, &sa, NULL);
}

void
excluding_puts(const char *str, const char *exc)
{
	for (size_t i = 0; str[i] != '\0'; i++) {
		int found = 0;
		for (size_t j = 0; exc[j] != '\0'; j++) {
			if (str[i] == exc[j]) {
				found = 1;
				break;
			}
		}
		if (!found)
			putc(str[i], stdout);
	}
}

void
print_status(void)
{
	for (size_t i = 0; i < LENGTH(procs); i++) {
		excluding_puts(procs[i].buffer, "\r\n");
		if (i < LENGTH(procs) - 1)
			excluding_puts(delimiter, "\n");
	}
	putc('\n', stdout);
	fflush(stdout);
}

int
update_buffer(Proc *p)
{
	char tmp[BUFFER];
	ssize_t r = read(p->sv, tmp, BUFFER - 1);
	die_if(r < 0, "read");
	tmp[r] = '\0';
	if (r == 0 || strcmp(tmp, p->buffer) == 0)
		return 0;

	strncpy(p->buffer, tmp, BUFFER);
	p->buffer[BUFFER - 1] = '\0';

	return 1;
}

void
reg_proc(Proc *p, int epfd)
{
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.ptr = p;
	epoll_ctl(epfd, EPOLL_CTL_ADD, p->sv, &ev);
}

void
fd_set_nonblock(int fd) {
	int flags = fcntl(fd, F_GETFL);
	die_if(flags < 0, "fcntl(F_GETFL)");
	die_if(
		fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0,
		"fcntl(F_SETFL)"
	);
}

int
make_proc(Proc *p)
{
	p->sv = -1;
	p->buffer[0] = '\0';
	p->pid = fork();
	die_if(p->pid < 0, "fork");
	if (p->pid == 0)
		return 1;
	return 0;
}

void
exec_memfd(int fd) {
	char path[64];
	snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);

	char *const argv[] = { path, NULL };
	execve(path, argv, environ);

	if (errno == ENOEXEC) {
		char *sh_argv[] = { "sh", path, NULL };
		execve("/bin/sh", sh_argv, environ);
	}

	perror("execve");
	close(fd);
	_exit(EXIT_FAILURE);
}

int
make_runnable_memfd(const char *cmd) {
	int fd = memfd_create("script", 0);
	if (fd < 0) return -1;
	if (write(fd, cmd, strlen(cmd)) < 0) {
		close(fd);
		return -1;
	}
	if (fchmod(fd, 0700) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

void
run_command(const char *cmd, int sv) {
	int fd = make_runnable_memfd(cmd);
	die_if(fd < 0, "make_runnable_memfd");
	dup2(sv, STDIN_FILENO);
	dup2(sv, STDOUT_FILENO);
	close(sv);
	exec_memfd(fd);
}

void
run_all(int epfd)
{
	for (size_t i = 0; i < LENGTH(specs); i++) {
		int sv[2];
		die_if(
			socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0,
			"socketpair"
		);
		if (make_proc(&procs[i])) {
			close(sv[1]);
			run_command(specs[i].command, sv[0]);
		}
		close(sv[0]);
		procs[i].sv = sv[1];
		reg_proc(&procs[i], epfd);
	}
}

void*
wait_events(void *arg)
{
	int epfd = *(int*)arg;
	struct epoll_event events[LENGTH(procs)];
	for (;;) {
		int nfds;
		do {
			nfds = epoll_wait(epfd, events, LENGTH(procs), -1);
		} while (nfds == -1 && errno == EINTR);
		die_if(nfds < 0, "epoll_wait");
		for (size_t i = 0; i < nfds; i++)
			if(update_buffer(events[i].data.ptr))
				print_status();
	}
	return NULL;
}

int
main(int argc, char *argv[])
{
	int epfd = epoll_create1(0);
	pthread_t epoll_thread;

	die_if(
		pthread_create(&epoll_thread, NULL, wait_events, &epfd),
		"pthread_create"
	);
	setup_signals();
	run_all(epfd);
	pthread_join(epoll_thread, NULL);
	return EXIT_SUCCESS;
}
