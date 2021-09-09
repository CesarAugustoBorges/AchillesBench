APP=fbdd
CC=gcc -Wall
RAM=ram
EXE=./$(APP)river
LIB=libfsp_client.so
LOGS=$(APP).log

FSP_SERVER_PATH=./fsocket/fsp_server.c
FSP_SERVER=fsp_server
FSP_STRUCTS_PATH=./fsocket/fsp_structs.c 
FSP_STRUCTS=fsp_structs
FSP_CLIENT_PATH=./fsocket/fsp_client.c
FSP_CLIENT=fsp_client

FBD_DEFINES_PATH=fbd_defines.c
FBD_DEFINES=fbd_defines

FBD_STRUCTS_PATH=fbd_structs.c fbd_defines.c
FAULT_LIBRARY_PATH=./fault/fault.c

FLAGS=-Wall	-g -O0 -L/usr/local
LIBRARIES=-lbdus -lpthread -lcrypto -lssl `pkg-config --cflags --libs glib-2.0` -lxxhash

all:fbdd ram

fbdd: $(FSP_SERVER_PATH) $(FSP_STRUCTS_PATH) 
	sudo mkdir -p "/var/lib/fsocket"
	sudo touch "/var/lib/fsocket/fault_injection_socket"
	$(CC) $(APP).c $(FBD_STRUCTS_PATH) $(FSP_SERVER_PATH) $(FSP_STRUCTS_PATH) $(FAULT_LIBRARY_PATH) $(LIBRARIES) $(FLAGS) -o $(EXE) 

ram:
	$(CC) $(RAM).c -lbdus -Wall -o $(RAM)

fserver:
	$(CC) fserver.c -Wall -o fserver

install: 
	$(CC) $(FBD_DEFINES_PATH) $(LIBRARIES) $(FLAGS) -c -fPIC
	$(CC) -shared $(FBD_DEFINES).o -o lib$(FBD_DEFINES).so
	sudo cp lib$(FBD_DEFINES).so /usr/lib && sudo chmod 0755 /usr/lib/lib$(FBD_DEFINES).so 
	sudo cp ./fbd_defines.h /usr/include && sudo chmod 0755 /usr/include/fbd_defines.h

	$(CC) $(FSP_STRUCTS_PATH) $(LIBRARIES) $(FLAGS) -c -fPIC
	$(CC) -shared $(FSP_STRUCTS).o -o lib$(FSP_STRUCTS).so
	sudo cp lib$(FSP_STRUCTS).so /usr/lib && sudo chmod 0755 /usr/lib/lib$(FSP_STRUCTS).so 
	sudo cp ./fsocket/fsp_structs.h /usr/include && sudo chmod 0755 /usr/include/fsp_structs.h

	$(CC) $(FSP_CLIENT_PATH)  $(LIBRARIES) $(FLAGS) -c -fPIC
	$(CC) -shared $(FSP_CLIENT).o -o lib$(FSP_CLIENT).so
	sudo cp lib$(FSP_CLIENT).so  /usr/lib && sudo chmod 0755 /usr/lib/lib$(FSP_CLIENT).so
	sudo cp ./fsocket/fsp_client.h /usr/include && sudo chmod 0755 /usr/include/fsp_client.h

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf $(EXE)
	rm -rf $(LOGS)
	rm -rf $(RAM)
