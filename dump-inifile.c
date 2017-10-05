#include <stdio.h>
#include <stdlib.h>
#include <nv.h>
#include "libsvc/inifile.h"


static void
usage(void)
{
	printf("usage: dump-inifile inifile\n");
	exit(EXIT_FAILURE);
}


int
main(int argc, char *argv[])
{
	nvlist_t *nvl;

	if (argc < 2)
		usage();

	nvl = inifile_parse(argv[1]);
	nvlist_fdump(nvl, stdout);
	nvlist_destroy(nvl);

	return EXIT_SUCCESS;
}
