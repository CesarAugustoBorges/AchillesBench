#include <stdint.h>
#include "fsp_structs.h"

#ifndef FSP_SERVER_HEADER
#define FSP_SERVER_HEADER

FSP_Request fsp_recvRequest(int fd);
int fsp_sendResponse(int fd, FSP_Response response);
void* fsp_startServer(void* device_path);
void* fsp_startServer_Thread(void* device_path, pthread_t *thread_id);

#endif
