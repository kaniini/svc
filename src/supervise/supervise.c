/*
 * This file is a part of svc.
 * svc-supervise -- supervise a child process and restart it if necessary.
 *
 * Copyright (c) 2017 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */


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
#include <termios.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <assert.h>
#include <getopt.h>
#include <err.h>


#include "libsvc/ipc.h"
#include "libsvc/uidgid.h"
#include "libsvc/childproc.h"
#include "libsvc/signal.h"


struct supervisor {
	struct childproc proc;

	bool exiting;

	int manager_fd;
	int signal_fd;
	int watch_fds;

	mode_t umask;
};


/*
 * Process a supervisor IPC kill command.
 */
static void
supervisor_ipc_kill(int manager_fd, const nvlist_t *nvl, struct supervisor *sup)
{
	nvlist_t *obj;

	(void) nvl;

	obj = nvlist_create(0);
	ipc_obj_prepare(obj, "kill", 0, true);

	childproc_kill(&sup->proc, true);
	childproc_setstate(&sup->proc, CHILDPROC_DOWN);

	nvlist_add_bool(obj, "success", true);

	nvlist_send(manager_fd, obj);
	nvlist_destroy(obj);
}


/*
 * Process a supervisor IPC restart command.
 */
static void
supervisor_ipc_restart(int manager_fd, const nvlist_t *nvl, struct supervisor *sup)
{
	nvlist_t *obj;

	(void) nvl;

	obj = nvlist_create(0);
	ipc_obj_prepare(obj, "restart", 0, true);

	sup->proc.restart_count = 0;

	childproc_kill(&sup->proc, true);
	childproc_start(&sup->proc);

	nvlist_add_bool(obj, "success", true);
	nvlist_add_number(obj, "pid", sup->proc.child_pid);

	nvlist_send(manager_fd, obj);
	nvlist_destroy(obj);
}


/*
 * Process a supervisor IPC status command.
 */
static void
supervisor_ipc_status(int manager_fd, const nvlist_t *nvl, struct supervisor *sup)
{
	nvlist_t *obj;

	(void) nvl;

	obj = nvlist_create(0);
	ipc_obj_prepare(obj, "status", 0, true);

	nvlist_add_string(obj, "prog_name", sup->proc.prog_name);

	if (sup->proc.dir_chroot)
		nvlist_add_string(obj, "dir_chroot", sup->proc.dir_chroot);

	if (sup->proc.dir_chdir)
		nvlist_add_string(obj, "dir_chdir", sup->proc.dir_chdir);

	nvlist_add_number(obj, "pid", sup->proc.child_pid);

	nvlist_add_number(obj, "uid", sup->proc.child_uid);
	nvlist_add_number(obj, "gid", sup->proc.child_gid);

	nvlist_add_number(obj, "restart_count", sup->proc.restart_count);

	nvlist_add_number(obj, "respawn_delay", sup->proc.respawn_delay);
	nvlist_add_number(obj, "respawn_max", sup->proc.respawn_max);
	nvlist_add_number(obj, "respawn_period", sup->proc.respawn_period);
	nvlist_add_number(obj, "respawn_last", sup->proc.respawn_last);

	nvlist_send(manager_fd, obj);
	nvlist_destroy(obj);
}


/* table must be alphabetically sorted! */
static const ipc_hdl_dispatch_t supervisor_dispatch_table[] = {
	{"kill", (ipc_hdl_dispatch_fn_t) supervisor_ipc_kill},
	{"restart", (ipc_hdl_dispatch_fn_t) supervisor_ipc_restart},
	{"status", (ipc_hdl_dispatch_fn_t) supervisor_ipc_status},
};


/*
 * Process a supervisor IPC.
 */
static void
supervisor_ipc(struct supervisor *sup)
{
	nvlist_t *nvl;
	ipc_obj_return_code_t rc;

	nvl = nvlist_recv(sup->manager_fd, 0);
	if (nvl == NULL)
	{
		/* XXX: IPC failure occured, maybe handle more gracefully */
		sup->watch_fds--;
		return;
	}

	rc = ipc_obj_dispatch(sup->manager_fd, nvl, supervisor_dispatch_table, ARRAY_SIZE(supervisor_dispatch_table), sup);
	if (rc != IPC_OBJ_OK)
		ipc_obj_error(sup->manager_fd, nvl, rc);

	nvlist_destroy(nvl);
}


/*
 * Prepare to run the supervisor.
 */
static void
supervisor_prepare(struct supervisor *sup)
{
	sigset_t sigs;

	assert(sup != NULL);

	signal_block();

	sigemptyset(&sigs);
	sigaddset(&sigs, SIGCHLD);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGQUIT);

	sup->signal_fd = signalfd(-1, &sigs, SFD_CLOEXEC);

	umask(sup->umask);
}


#define SVC_SIGMAX (8 * sizeof(sigset_t) + 1)
typedef bool (*sighdl_fn_t)(struct supervisor *sup);


static bool
sighdl_chld(struct supervisor *sup)
{
	bool should_restart = childproc_monitor(&sup->proc);

	if (!should_restart)
	{
		childproc_setstate(&sup->proc, CHILDPROC_STOPPING);
		childproc_monitor(&sup->proc);

		sup->exiting = true;
		return true;
	}

	if (sup->proc.respawn_delay)
		return true;

	childproc_start(&sup->proc);
	return false;
}


static bool
sighdl_term(struct supervisor *sup)
{
	sup->exiting = true;
	childproc_setstate(&sup->proc, CHILDPROC_DOWN);
	childproc_kill(&sup->proc, true);
	return true;
}


static const sighdl_fn_t sighdl_fns[SVC_SIGMAX] = {
	[SIGCHLD] = sighdl_chld,
	[SIGTERM] = sighdl_term,
	[SIGQUIT] = sighdl_term
};


/*
 * Main supervision loop.
 */
static void
supervisor_run(struct supervisor *sup)
{
	bool pending_restart = false;

	assert(sup != NULL);

	childproc_start(&sup->proc);

	while (!sup->exiting)
	{
		struct pollfd pfds[2] = {
			[0] = {.fd = sup->signal_fd, .events = POLLIN},
			[1] = {.fd = sup->manager_fd, .events = POLLIN}
		};

		if (sup->proc.state != CHILDPROC_UP || sup->proc.state != CHILDPROC_READY)
			childproc_setstate(&sup->proc, CHILDPROC_UP);

		if (poll(pfds, sup->watch_fds, !pending_restart ? -1 : (sup->proc.respawn_delay * 1000)) < 0)
			abort();

		if (pfds[1].revents & POLLIN)
			supervisor_ipc(sup);

		if (pfds[0].revents & POLLIN)
		{
			struct signalfd_siginfo si;

			if (read(sup->signal_fd, &si, sizeof si) < (ssize_t) sizeof(si))
				abort();

			if (si.ssi_signo != SIGCHLD)
				continue;

			if (sighdl_fns[si.ssi_signo] != NULL)
			{
				pending_restart = sighdl_fns[si.ssi_signo](sup);
				if (pending_restart)
					continue;
			}
		}

		/* if a restart was enqueued, handle it now */
		if (pending_restart)
		{
			childproc_start(&sup->proc);
			pending_restart = false;
		}
	}
}


static void
usage(void)
{
	printf("usage: svc-supervise [options] -- [program] [arguments]\n\nOptions:\n\n");

	printf("    --help                        this message\n");
	printf("    --stdout=PATH                 redirect program stdout to PATH\n");
	printf("    --stderr=PATH                 redirect program stderr to PATH\n");
	printf("    --chdir=PATH                  change directory to PATH\n");
	printf("    --chroot=PATH                 change root directory to PATH\n");
	printf("    --respawn-delay=SECONDS       wait SECONDS before respawning\n");
	printf("    --respawn-max=NUMBER          give up respawning after NUMBER times\n");
	printf("    --manager-fd=NUMBER           perform manager-supervisor IPC on the given\n");
	printf("                                  descriptor number\n");
	printf("    --umask=UMASK                 set supervisor umask\n");

	exit(EXIT_SUCCESS);
}


const char *shortopts = "D:m:d:r:e:1:2:u:g:h";
const struct option longopts[] = {
	{"respawn-delay",	1, NULL, 'D'},
	{"respawn-max",		1, NULL, 'm'},
	{"chdir",		1, NULL, 'd'},
	{"chroot",		1, NULL, 'r'},
#ifdef NOTYET
	{"env",			1, NULL, 'e'},
#endif
	{"stdout",		1, NULL, '1'},
	{"stderr",		1, NULL, '2'},
	{"uid",			1, NULL, 'u'},
	{"gid",			1, NULL, 'g'},
	{"umask",		1, NULL, 'k'},
	{"help",		0, NULL, 'h'},
	{"manager-fd",		1, NULL, 128},
	{NULL,			0, NULL, 0  },
};


static void
redirect_descriptor(int *des, const char *path)
{
	int fileno;

	assert(des != NULL);
	assert(path != NULL);

	fileno = open(path, O_CREAT | O_APPEND | O_RDWR);
	if (fileno < 0)
		err(1, "redirection of %s", path);

	*des = fileno;
}


/*
 * Set up the supervisor object and begin supervision.
 */
int
main(int argc, char *argv[])
{
	int ret;
	struct supervisor sup = {};

	sup.exiting = false;
	sup.manager_fd = -1;
	sup.watch_fds = 1;
	sup.umask = 022;

	sup.proc.child_uid = -1;
	sup.proc.child_gid = -1;

	if (argc < 2)
		usage();

	/* set up the supervisor object */
	while ((ret = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch (ret)
		{
			case 'h':
				usage();
				break;

			case '1':
				redirect_descriptor(&sup.proc.stdout_fd, optarg);
				break;

			case '2':
				redirect_descriptor(&sup.proc.stderr_fd, optarg);
				break;

			case 'd':
				sup.proc.dir_chdir = optarg;
				break;

			case 'r':
				sup.proc.dir_chroot = optarg;
				break;

			case 'D':
				sup.proc.respawn_delay = atoi(optarg);
				break;

			case 'm':
				sup.proc.respawn_max = atoi(optarg);
				break;

			case 'u':
				sup.proc.child_uid = uid_resolve(optarg);
				if (sup.proc.child_uid == -1)
				{
					fprintf(stderr, "%s: could not resolve user: %s, aborting\n", argv[0], optarg);
					return EXIT_FAILURE;
				}

				break;

			case 'g':
				sup.proc.child_gid = gid_resolve(optarg);
				if (sup.proc.child_gid == -1)
				{
					fprintf(stderr, "%s: could not resolve group: %s, aborting\n", argv[0], optarg);
					return EXIT_FAILURE;
				}

				break;

			case 'k':
				parse_mode(&sup.umask, optarg);
				break;

			case 128:
				sup.manager_fd = atoi(optarg);
				sup.watch_fds++;
				break;

			default:
				fprintf(stderr, "unhandled argument: %d\n", ret);
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	sup.proc.prog_name = argv[0];
	sup.proc.prog_argv = argv;
	sup.proc.kill_delay = 3;

	/* TODO: add optional detach */
	supervisor_prepare(&sup);
	supervisor_run(&sup);

	return EXIT_SUCCESS;
}
