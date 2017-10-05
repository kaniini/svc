lib_LTLIBRARIES = libsvc.la


LIBS += $(LIBNV_LIBS)
CPPFLAGS = -Isrc $(LIBNV_CFLAGS)
libsvc_la_SOURCES = 			\
	src/libsvc/childproc.c		\
	src/libsvc/inifile.c		\
	src/libsvc/ipc.c		\
	src/libsvc/signal.c		\
	src/libsvc/uidgid.c


sbin_PROGRAMS = svc-supervise
svc_supervise_SOURCES = src/supervise/supervise.c
svc_supervise_LDADD = libsvc.la


noinst_PROGRAMS = dump-inifile
dump_inifile_SOURCES = dump-inifile.c
dump_inifile_LDADD = libsvc.la
