#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/queue.h>


#ifndef LIBSVC_CHILDPROC_H
#define LIBSVC_CHILDPROC_H

typedef enum childproc_state_e {
	CHILDPROC_INITIAL,
	CHILDPROC_STARTING,
	CHILDPROC_UP,
	CHILDPROC_READY,
	CHILDPROC_CRASHED,
	CHILDPROC_STOPPING,
	CHILDPROC_DOWN
} childproc_state_t;


struct childproc {
	char *prog_name;
	char **prog_argv;

	char *dir_chroot;
	char *dir_chdir;

	int restart_count;

	int respawn_delay;
	int respawn_max;
	int respawn_period;
	time_t respawn_last;

	int kill_delay;

	pid_t child_pid;

	uid_t child_uid;
	gid_t child_gid;

	int stdin_fd;
	int stdout_fd;
	int stderr_fd;

	childproc_state_t state;
};


void childproc_setstate(struct childproc *proc, childproc_state_t state);
void childproc_start(struct childproc *proc);
void childproc_exec(struct childproc *proc);
bool childproc_kill(struct childproc *proc, bool should_wait);
bool childproc_monitor(struct childproc *proc);

#endif
