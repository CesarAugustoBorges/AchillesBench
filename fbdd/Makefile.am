bin_PROGRAMS=fbddriver		
fbddriver_SOURCES=fbdd.c fbd_structs.c ./fault/fault.c ./fsocket/fsp_server.c
fbddriver_LDADD=-lbdus -lpthread -lcrypto -lssl -lglib-2.0 -lfsp_client -lfbd_defines -lfsp_structs
fbddriver_LDFLAGS=$(GLIB_LIBS)
fbddriver_CFLAGS=$(GLIB_CFLAGS)
