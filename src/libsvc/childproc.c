/* childproc helpers */

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
#include <assert.h>


#include "libsvc/childproc.h"
#include "libsvc/signal.h"


/*
 * Set childproc state.
 */
void
childproc_setstate(struct childproc *proc, childproc_state_t state)
{
	proc->state = state;
}


/*
 * Fork a child process to execute the service in, via childproc_exec().
 */
void
childproc_start(struct childproc *proc)
{
	assert(proc != NULL);

	childproc_setstate(proc, CHILDPROC_STARTING);

	proc->child_pid = fork();
	if (proc->child_pid == 0)
	{
		childproc_exec(proc);
		exit(EXIT_FAILURE);
	}

	proc->respawn_last = time(NULL);
}


/*
 * Execute a child process.
 */
void
childproc_exec(struct childproc *proc)
{
	assert(proc != NULL);

	signal_unblock();

	setsid();

	/* indicate to the system operator that the process is alive */
	syslog(LOG_INFO, "%s: starting, pid %d", proc->prog_name, getpid());

	if (proc->dir_chroot != NULL && chroot(proc->dir_chroot) < 0)
	{
		syslog(LOG_INFO, "%s: failed to chroot to '%s': %s", proc->prog_name, proc->dir_chroot, strerror(errno));
		return;
	}

	if (proc->dir_chdir != NULL && chdir(proc->dir_chdir) < 0)
	{
		syslog(LOG_INFO, "%s: failed to chdir to '%s': %s", proc->prog_name, proc->dir_chdir, strerror(errno));
		return;
	}

	if (proc->child_gid > -1 && setgid(proc->child_gid))
	{
		syslog(LOG_INFO, "%s: failed to setgid to %d: %s", proc->prog_name, proc->child_gid, strerror(errno));
		return;
	}

	if (proc->child_uid > -1 && setgid(proc->child_uid))
	{
		syslog(LOG_INFO, "%s: failed to setuid to %d: %s", proc->prog_name, proc->child_uid, strerror(errno));
		return;
	}

	dup2(proc->stdin_fd, STDIN_FILENO);
	dup2(proc->stdout_fd, STDOUT_FILENO);
	dup2(proc->stderr_fd, STDERR_FILENO);

	for (int i = getdtablesize() - 1; i > STDERR_FILENO; i--)
		fcntl(i, F_SETFD, FD_CLOEXEC);

	execvp(proc->prog_name, proc->prog_argv);
	syslog(LOG_INFO, "%s: failed to exec %s: %s", proc->prog_name, proc->prog_name, strerror(errno));
}


/*
 * Kill a process.
 */
bool
childproc_kill(struct childproc *proc, bool should_wait)
{
	int i;

	assert(proc != NULL);
	assert(proc->child_pid != 0);

	kill(proc->child_pid, SIGTERM);

	if (!should_wait)
		return true;

	waitpid(proc->child_pid, &i, WNOHANG);

	if (WIFEXITED(i) || WIFSIGNALED(i))
		return true;

	sleep(proc->kill_delay);

	waitpid(proc->child_pid, &i, WNOHANG);

	if (WIFEXITED(i) || WIFSIGNALED(i))
		return true;

	kill(proc->child_pid, SIGKILL);

	waitpid(proc->child_pid, &i, 0);

	return WIFEXITED(i) != 0 || WIFSIGNALED(i) != 0;
}


/*
 * Monitor a child process using wait(2).
 * Returns true if process needs to be restarted, else false.
 */
bool
childproc_monitor(struct childproc *proc)
{
	int i;

	assert(proc != NULL);
	assert(proc->child_pid != 0);

	waitpid(proc->child_pid, &i, 0);

	if (proc->state == CHILDPROC_STOPPING || proc->state == CHILDPROC_DOWN)
	{
		signal(SIGCHLD, SIG_IGN);
		syslog(LOG_INFO, "%s: stop%s, pid %d", proc->prog_name, proc->state == CHILDPROC_DOWN ? "ed" : "ping", proc->child_pid);

		if (proc->state != CHILDPROC_DOWN)
		{
			childproc_kill(proc, true);
			childproc_setstate(proc, CHILDPROC_DOWN);
		}
	}
	else
	{
		time_t current_ts = time(NULL);

		childproc_setstate(proc, CHILDPROC_CRASHED);

		proc->restart_count++;
		if (proc->respawn_period && current_ts - proc->respawn_last > proc->respawn_period)
			proc->restart_count = 0;

		if (proc->respawn_max > 0 && proc->restart_count > proc->respawn_max)
		{
			syslog(LOG_INFO, "%s: restarted too many times, giving up", proc->prog_name);
			return false;
		}

		return true;
	}

	return false;
}
