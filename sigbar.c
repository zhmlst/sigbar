#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>

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

static Proc procs[LENGTH(specs)];

void
die_on_err(ssize_t val, const char *err)
{
	if (val < 0) {
		perror(err);
		exit(EXIT_FAILURE);
	}
}

void
handle_signal(int s)
{
	for (size_t i = 0; i < LENGTH(procs); i++)
		if (specs[i].signal == s - SIGRTMIN)
			die_on_err(kill(procs[i].pid, SIGRTMIN), "kill");
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
	die_on_err(r, "read");
	tmp[r] = '\0';
	if (r == 0 || strcmp(tmp, p->buffer) == 0)
		return 0;

	strncpy(p->buffer, tmp, BUFFER);
	p->buffer[BUFFER - 1] = '\0';

	return 1;
}

void
wait_events(int epfd)
{
	struct epoll_event events[LENGTH(procs)];
	for (;;) {
		int nfds;
		do {
			nfds = epoll_wait(epfd, events, LENGTH(procs), -1);
		} while (nfds == -1 && errno == EINTR);

		die_on_err(nfds, "epoll_wait");
		for (size_t i = 0; i < nfds; i++)
			if(update_buffer(events[i].data.ptr))
				print_status();
	}
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
	die_on_err(flags, "fcntl(F_GETFL):");
	die_on_err(fcntl(fd, F_SETFL, flags | O_NONBLOCK), "fcntl(F_SETFL):");
}

void
run_command(int sv, const char *c)
{
	dup2(sv, STDIN_FILENO);
	dup2(sv, STDOUT_FILENO);
	close(sv);
	execl("/bin/sh", "sh", "-c", c, NULL);
	perror("execl");
	_exit(EXIT_FAILURE);
}

void
spawn_proc(Proc *p, const Spec *s)
{
	int sv[2];
	die_on_err(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), "socketpair");
	p->pid = fork();
	die_on_err(p->pid, "fork");
	if (p->pid == 0) {
		close(sv[0]);
		run_command(sv[1], s->command);
	}
	close(sv[1]);
	p->sv = sv[0];
	fd_set_nonblock(p->sv);
}

int
main(int argc, char *argv[])
{
	setup_signals();
	int epfd = epoll_create1(0);
	for (size_t i = 0; i < LENGTH(specs); i++) {
		spawn_proc(&procs[i], &specs[i]);
		reg_proc(&procs[i], epfd);
	}
	wait_events(epfd);
	return 0;
}
