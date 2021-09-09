#include "fbd_defines.h"

char* fbd_operation_to_string(int op){
    switch (op){
    case FBD_OP_NONE:
        return "none";
    case FBD_OP_READ:
        return "read";
    case FBD_OP_WRITE:
        return "write";
    case FBD_OP_WRITE_READ:
        return "write/read";
    default:
        return "not_found";
    }
}

char* fbd_fault_to_string(int f){
    switch (f){
    case FBD_FAULT_NONE:
        return "none";
    case FBD_FAULT_BIT_FLIP:
        return "bit_blip";
    case FBD_FAULT_MEDIUM:
        return "medium_error";
    case FBD_FAULT_SLOW_DISK:
        return "slow_disk";
    default:
        return "not_found";
    }
}

char* fbd_mode_to_string(int mode){
    switch (mode){
    case FBD_MODE_BLOCK:
        return "block";
    case FBD_MODE_HASH:
        return "hash";
    case FBD_MODE_DEDUP:
        return "dedup";
    case FBD_MODE_DEVICE:
        return "device";
    case FBD_MODE_RESET_ALL:
        return "remove_faults";
    default:
        return "not_found";
    }
}

char* fbd_response_to_string(int res){
    switch (res){
    case FBD_STS_CONN_CLOSED:
        return "Connection closed"; 
    case FBD_STS_OK:
        return "OK";
    case FBD_STS_DUP_FAULT:
        return "Duplicated fault";
    case FBD_STS_ERROR:
        return "Error ocurred";
    case FBD_STS_INVALID_MODE:
        return "Unacceptale mode";
    case FBD_STS_NOT_FOUND:
        return "Fault not found";
    case FBD_STS_MEDIUM_ERROR:
        return "Medium error";
    case FBD_STS_WRONG_MODE:
        return "Not recognizable mode";
    case FBD_STS_SEND_FAILED:
        return "Error in sending request";
    case FBD_STS_WRONG_INPUT:
        return "Invalid inputs";
    case FBD_STS_RECV_FAILED:
        return "Error in receiving request";
    case FBD_STS_INVALID_HASH_TYPE:
        return "Invalid hash type";
    default:
        return "Response code not found";
    }
}