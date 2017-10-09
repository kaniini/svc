/* helpers for processing nvlists */

#include <nv.h>


#ifndef LIBSVC_NVLIST_PROCESS_H
#define LIBSVC_NVLIST_PROCESS_H


typedef void (*nvlist_process_fn_t)(const char *key, const nvpair_t *pair, void *opaque);

typedef struct nvlist_process_table_s {
	const char *key;
	const nvlist_process_fn_t dispatch_fn;
	const int type;
} nvlist_process_table_t;


void nvlist_process(const nvlist_t *nvl, const nvlist_process_table_t table[], size_t table_size, void *opaque);


#endif
