#include <stdio.h>
#include <stdbool.h>


#ifndef LIBSVC_ARGVSPLIT_H
#define LIBSVC_ARGVSPLIT_H


typedef struct argv_s {
	int count;
	char **argv;
} argv_t;


void argv_destroy(argv_t *argv);
void argv_free(argv_t *argv);
bool argv_split(argv_t *argv, const char *src);
void argv_append(argv_t *argv, const char *arg);
const char * const *argv_pack(const argv_t *argv);
int argv_count(const argv_t *argv);
void argv_fdump(const argv_t *argv, FILE *f);


#endif
