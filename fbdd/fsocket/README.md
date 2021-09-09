# FSocket

FSocket is a middleware responsible to establish the communication protocol of **FConsole** or another application with a server responsible to manage faults. An *Unix Socket* is created, it uses a file descriptor as a stream to exchange data between the two processes.

# fsd_client.h

Includes all functions needed to create a file descriptor, connect to the server, send a request and receive a response. 
This module only sends requests, it doesn't inject the faults, these are injected in **Fault Library** *(fault/fault.h)*, for more information about he injected faults consult the **Fault Library** *README.md*.For now, this API allows the client to:
 - *fsp\_socket* - retrieves the socket used as a stream;
 - *fsp\_connect* - connects to the server responsible to manage faults;
 - *fsp\_add\_bit\_flip\_write* - sends a bit flip fault on write request;
 - *fsp\_add\_bit\_flip\_read* - sends a bit flip fault on read request;
 - *fsp\_add\_bit\_flip\_WR* - sends a bit flip fault on write and read requests;
 - *fsp\_add\_slow\_disk\_write* - sends a slow disk fault on write request;
 - *fsp\_add\_slow\_disk\_read* - sends a slow disk fault on read request;
 - *fsp\_add\_slow\_disk\_WR* - sends a slow disk fault on write and read requests;
 - *fsp\_add\_medium\_error\_write* - sends a medium error fault on write request;
 - *fsp\_add\_medium\_error\_read* - sends a medium error fault on read request;
 - *fsp\_add\_medium\_error\_WR* - sends a medium error disk fault on write and read requests;


All *fsp\_add\_bit\_flip\**, *fsp\_add\_slow\_disk\**, *fsp\_add\_medium\_error*, faults are injected in the future, when the appropriate operation is called (write/read).

# fsd_server.h
It's reponsability is to process all clients requests and give them a response, this includes receiving the faults requests from the clients and inserting the necessary data into a structure that **FBDD** as acess to. For now, this is a simple black blox API:
 - *fsp_startServer* - initiates the server.
 - *fsp_startServer_Thread* - initiates the server in a thread.
