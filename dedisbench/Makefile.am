AUTOMAKE_OPTIONS = subdir-objects
CXXFLAGS = -Wall -Iutils/random/randomgen 
bin_PROGRAMS=DEDISbench DEDISgen DEDISgenutils
DEDISbench_SOURCES= benchcore/faults/dedupDegree.h utils/random/random.c utils/db/berk.c structs/structs.h benchcore/duplicates/duplicatedist.c benchcore/faults/configParserYaml.c benchcore/faults/dedupDegree.c benchcore/faults/fault.c benchcore/accesses/iodist.c benchcore/io.c populate/populate.c benchcore/sharedmem/sharedmem.c DEDISbench.c parserconf/inih/ini.c io/plotio.c utils/utils.c
DEDISbench_CFLAGS= -Wall -Iutils/random/randomgen $(GLIB_CFLAGS) -I/usr/includes -DINI_INLINE_COMMENT_PREFIXES=\"\#\"
DEDISbench_LDADD = -lcrypto -lssl -lbdus -lpthread -lcrypto -lssl -lfsp_client -lfsp_structs -lfbd_defines -lglib-2.0 -lyaml -lxxhash $(GLIB_LIBS)
DEDISgen_SOURCES=DEDISgen.c utils/db/berk.c
DEDISgen_CFLAGS = -Wall $(GLIB_CFLAGS)
DEDISgen_LDADD= $(GLIB_LIBS)
DEDISgenutils_SOURCES=DEDISgen-utils.c utils/db/berk.c
DEDISgenutils_CFLAGS = -Wall $(GLIB_CFLAGS)
DEDISgenutils_LDADD= $(GLIB_LIBS)