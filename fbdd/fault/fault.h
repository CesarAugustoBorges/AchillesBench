#include <stdint.h>

#ifndef FAULTY_LYBRARY_HEADER
#define FAULTY_LYBRARY_HEADER

int fl_inject_slow_disk_fault(uint64_t ms);
int fl_inject_bit_flip_fault_buffer(char* buffer, uint32_t size);
int fl_inject_medium_disk_fault();

#endif