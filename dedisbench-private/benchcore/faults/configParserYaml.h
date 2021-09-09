#ifndef CONFIG_PARSER_YAML_H
#define CONFIG_PARSER_YAML_H

#include <stdio.h>
#include <yaml.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>

#include "../duplicates/duplicatedist.h"
#include "fault.h"
#include "../../structs/structs.h"

#define MAX_KEY_STACK_SIZE 10

#define NONE_STR "none"


//MODE
#define MODE_STR        "mode"
#define MODE_HASH_STR   "mode_hash"
#define MODE_OFFSET_STR "mode_offset"
#define MODE_DEDUP_STR  "mode_dedup"
#define MODE_DISK_STR   "mode_disk"

//operation tokens
#define WRITE_STR "write"
#define READ_STR "read"
#define WRITE_READ_STR "write_read"

//faults tokens
#define MEDIUM_ERROR_STR "medium_error"
#define SLOW_DISK_STR "slow_disk"
#define BIT_FLIP_STR "bit_flip"
#define TYPE_STR "type"
#define OPERATION_STR "operation"
#define WHEN_OPERATION_STR "when_operation"
#define WHEN_TIME_STR "when_time"
#define TARGET_STR "target"

//Extra tokens
#define DELAY_STR "delay"


//Other tokens
#define FAULTS_CONF_STR "faults"
#define AVOID_REPEATABLE_FAULTS_STR "avoid_repeatable_faults"
#define LENGTH_FAULTS_STR "length_faults"

//Target tokens
#define LAST_BLOCK_STR "last_block"
#define TOP_BLOCK_STR "top_block"
#define NEXT_BLOCK_STR "next_block"
#define UNIQUE_BLOCK_STR "unique_block"

//extra types 
#define FAULT_CONF_EXTRA_NONE 0
#define FAULT_CONF_EXTRA_TIME 1

//persistency
#define PERSISTENCE_STR "persistent"
#define TRANSIENT_STR "transient"

//Yes and No
#define YES_STR "yes"
#define NO_STR "no"

//default values
#define FAULT_CONF_EXTRA_TIME_DEFAULT 1

typedef struct sorted_fault_confs {
    GArray *by_time;
    GArray *by_operation;

    uint32_t itime;
    uint32_t ioperation;
} *Sorted_Fault_Confs;

int parse_faults_configuration_yaml(struct user_confs* conf, char* conf_file_path, struct faults_info* info);

#endif
