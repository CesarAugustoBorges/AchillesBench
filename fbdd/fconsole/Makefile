APP=fconsole
LIBS=-lfsp_client -lpthread -lcrypto -lssl `pkg-config --cflags --libs glib-2.0` -lxxhash
FSP_CLIENT=../fsocket/fsp_client.c ../fsweeper/fsweeper.c
FSP_STRUCTS=../fsocket/fsp_structs.c
FBD_DEFINES=../fbd_defines.c

all: fconsole.o

fconsole.o: fconsole.c fdevice.c $(FSP_STRUCTS) $(FSP_CLIENT) $(FBD_DEFINES)
	gcc fconsole.c 	fdevice.c $(FSP_STRUCTS) $(FSP_CLIENT) $(FBD_DEFINES) $(LIBS) -Wall -g -o  $(APP) 

clean:
	rm -rf *.o
	rm -rf ./fconsole