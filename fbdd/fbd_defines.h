#ifndef FBD_DEFINES_HEADER
#define FBD_DEFINES_HEADER


//Operations
#define FBD_OP_NONE 0
#define FBD_OP_WRITE 1
#define FBD_OP_READ 2
#define FBD_OP_WRITE_READ 3

//Modes
#define FBD_MODE_BLOCK 0
#define FBD_MODE_HASH 1
#define FBD_MODE_DEDUP 2
#define FBD_MODE_DEVICE 3 //Device mode is converted to block mode in fsp_server
#define FBD_MODE_RESET_ALL 4 //Removes all faults previously defined


//Faults type
#define FBD_FAULT_NONE 0
#define FBD_FAULT_BIT_FLIP 1
#define FBD_FAULT_SLOW_DISK 2
#define FBD_FAULT_MEDIUM 3

//Response Status
#define FBD_STS_CONN_CLOSED 0
#define FBD_STS_CACHE_NOT_FOUND 0
#define FBD_STS_OK 1
#define FBD_STS_ERROR -1
#define FBD_STS_NOT_FOUND -2
#define FBD_STS_SEND_FAILED -3
#define FBD_STS_WRONG_INPUT -4
#define FBD_STS_DUP_FAULT 2
#define FBD_STS_MEDIUM_ERROR -5
#define FBD_STS_WRONG_MODE -6
#define FBD_STS_INVALID_MODE -7
#define FBD_STS_RECV_FAILED -8
#define FBD_STS_INVALID_HASH_TYPE -9

//OThers
#define MAX_FAULTS 3

//Hashes
#define FBD_HASH_MD5            0
#define FBD_HASH_XXH3_128       1 
#define FBD_HASH_MURMUR_x86_128 2

char* fbd_operation_to_string(int op);
char* fbd_fault_to_string(int f);
char* fbd_mode_to_string(int mode);
char* fbd_response_to_string(int res);

#endif