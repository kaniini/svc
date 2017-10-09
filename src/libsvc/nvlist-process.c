#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "libsvc/nvlist-process.h"


static int
nvlist_process_table_cmp(const char *key, const void *tentry)
{
	const nvlist_process_table_t *ptentry = tentry;
	return strcmp(key, ptentry->key);
}


void
nvlist_process(const nvlist_t *nvl, const nvlist_process_table_t table[], size_t table_size, void *opaque)
{
	nvpair_t *nvp;
	const char *key;
	const nvlist_process_table_t *tentry;

	assert(nvl != NULL);
	assert(table_size > 0);

	nvp = nvlist_first_nvpair(nvl);
	while (nvp != NULL)
	{
		key = nvpair_name(nvp);
		assert(key != NULL);

		tentry = bsearch(key, table, table_size, sizeof(nvlist_process_table_t), (void *) nvlist_process_table_cmp);
		if (tentry == NULL)
			goto next;

		if (nvpair_type(nvp) != tentry->type)
			goto next;

		assert(tentry->dispatch_fn != NULL);
		tentry->dispatch_fn(key, nvp, opaque);

next:
		nvp = nvlist_next_nvpair(nvl, nvp);
	}
}
