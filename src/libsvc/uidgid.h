#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef LIBSVC_UIDGID_H
#define LIBSVC_UIDGID_H

uid_t uid_resolve(const char *username);
gid_t gid_resolve(const char *groupname);

#endif
