#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <nv.h>

#ifndef LIBSVC_IPC_H
#define LIBSVC_IPC_H

#include "libsvc/common.h"

typedef enum ipc_obj_return_code_e {
	IPC_OBJ_OK,
	IPC_OBJ_INVALID,
	IPC_OBJ_IS_REPLY,
	IPC_OBJ_METHOD_NOT_FOUND,
} ipc_obj_return_code_t;

typedef ipc_obj_return_code_t (*ipc_hdl_dispatch_fn_t)(int sock, const nvlist_t *nvl, void *opaque);
typedef struct ipc_hdl_dispatch_s {
	const char *method;
	ipc_hdl_dispatch_fn_t dispatch_fn;
} ipc_hdl_dispatch_t;

bool ipc_obj_is_reply(const nvlist_t *nvl);
bool ipc_obj_validate(const nvlist_t *nvl);

void ipc_obj_prepare(nvlist_t *nvl, const char *method, uint64_t id, bool reply);
ipc_obj_return_code_t ipc_obj_dispatch(int sock, const nvlist_t *nvl, const ipc_hdl_dispatch_t dispatch_table[], size_t dispatch_table_size, void *opaque);

#endif
