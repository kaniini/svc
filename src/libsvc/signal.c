#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


void
signal_block(void)
{
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
		abort();
}


void
signal_unblock(void)
{
	sigset_t mask;

	sigemptyset(&mask);

	if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
		abort();
}
