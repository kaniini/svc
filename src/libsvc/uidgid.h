#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef LIBSVC_UIDGID_H
#define LIBSVC_UIDGID_H

uid_t uid_resolve(const char *username);
gid_t gid_resolve(const char *groupname);
int parse_mode(mode_t *mode, const char *text);

#endif
