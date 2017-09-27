lib_LTLIBRARIES = libsvc.la


libsvc_la_CPPFLAGS = -Isrc $(LIBNV_CFLAGS)
libsvc_la_SOURCES = 			\
	src/libsvc/ipc.c


sbin_PROGRAMS = svc-supervise
svc_supervise_SOURCES = src/supervise/supervise.c
svc_supervise_LDADD = libsvc.la
