#include <stdbool.h>

#include "../fsocket/fsp_client.h"

bool fw_sweep_block(int fd, char* buf, int size, uint64_t offSet, char* expected_content);