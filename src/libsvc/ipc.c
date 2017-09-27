/* IPC helpers */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <nv.h>


#include "libsvc/ipc.h"


void
ipc_obj_prepare(nvlist_t *nvl, const char *method, uint64_t id, bool reply)
{
	nvlist_add_number(nvl, "ipc:version", 1);
	nvlist_add_number(nvl, "ipc:id", id);
	nvlist_add_string(nvl, "ipc:method", method);
	nvlist_add_bool(nvl, "ipc:reply", reply);
}


bool
ipc_obj_is_reply(const nvlist_t *nvl)
{
	if (!nvlist_exists_bool(nvl, "ipc:reply"))
		return false;

	return nvlist_get_bool(nvl, "ipc:reply");
}


bool
ipc_obj_validate(const nvlist_t *nvl)
{
	if (!nvlist_exists_bool(nvl, "ipc:reply") || !nvlist_exists_string(nvl, "ipc:method") ||
		!nvlist_exists_number(nvl, "ipc:id") || !nvlist_exists_number(nvl, "ipc:version"))
		return false;

	return true;
}


static int
ipc_obj_dispatch_method_cmp(const char *key, const void *tentry)
{
	const ipc_hdl_dispatch_t *dtentry = tentry;
	return strcmp(key, dtentry->method);
}


ipc_obj_return_code_t
ipc_obj_dispatch(int sock, const nvlist_t *nvl, const ipc_hdl_dispatch_t dispatch_table[], size_t dispatch_table_size)
{
	const char *method;
	const ipc_hdl_dispatch_t *pair;

	if (!ipc_obj_validate(nvl))
		return IPC_OBJ_INVALID;

	if (ipc_obj_is_reply(nvl))
		return IPC_OBJ_IS_REPLY;

	method = nvlist_get_string(nvl, "ipc:method");
	if (method == NULL)
		return IPC_OBJ_INVALID;

	pair = bsearch(method, dispatch_table, dispatch_table_size, sizeof(ipc_hdl_dispatch_t), (void *) ipc_obj_dispatch_method_cmp);
	if (pair == NULL || pair->dispatch_fn == NULL)
		return IPC_OBJ_METHOD_NOT_FOUND;

	return pair->dispatch_fn(sock, nvl);
}
