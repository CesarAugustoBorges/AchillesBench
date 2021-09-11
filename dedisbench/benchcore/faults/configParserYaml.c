#include <stdlib.h>
#include <fbd_defines.h>


#include "configParserYaml.h"
#include "fault.h"

#define current_fault_conf(data) &data->current_fault
#define current_index_conf(data) &data->ifaults[data->current_procid]
#define current_size_conf(data) &data->nfaults[data->current_procid]

#define get_fault_conf_index(data, i) &data->fault_conf[data->current_procid][i]
#define get_fault_conf_size(data) data->nfaults[data->current_procid]
#define get_fault_conf_index_of_procid(data, i) &data->ifaults[i]
#define get_fault_conf_procid(data, procid) data->fault_conf[procid]

void reset_fault_conf(Fault_Conf fault_conf){
    fault_conf->mode       = FBD_MODE_HASH;
    fault_conf->operation  = -1;
    fault_conf->when       = -1;
    fault_conf->fault_dist = -1;
    fault_conf->fault_type = -1;
    fault_conf->proc_id    = -1;
    fault_conf->measure    = -1;
    fault_conf->extra_type = FAULT_CONF_EXTRA_NONE;
    fault_conf->extra      = NULL;
}

bool validate_fault_conf(Fault_Conf fc){
    if(fc->extra_type == FAULT_CONF_EXTRA_TIME && fc->extra == NULL){
        uint32_t *delay = malloc(sizeof(uint32_t));
        *delay = FAULT_CONF_EXTRA_TIME_DEFAULT;
        fc->extra = delay;
    }
    return fc->operation >= 0 && fc->when >= 0 && fc->fault_dist >= 0 &&
            fc->fault_type >= 0 /*&& fc->proc_id < 0*/ && fc->measure >= 0;
}

typedef struct fault_conf_metadata {
    bool in_faults_scalar;
    bool in_faults_sequence;
    bool in_fault_def_block;
    int current_procid;
    char* last_key;
    GArray *fault_conf;
    struct fault_conf current_fault;
    bool is_key;
    char *key_stack[MAX_KEY_STACK_SIZE];
    uint8_t key_stack_size;

    bool avoid_repeatable_faults;
} *Fault_Conf_Metadata;


void init_fault_conf_metadata(Fault_Conf_Metadata data, int nprocs){
    data->in_faults_scalar   = false;
    data->in_faults_sequence = false;
    data->in_fault_def_block = false;
    data->last_key           = NULL;
    reset_fault_conf(&data->current_fault);      
    data->is_key             = false;
    data->key_stack_size     = 0;
    data->current_procid     = 0;
    //data->ifaults    = malloc(sizeof(int) * nprocs);
    //data->nfaults    = malloc(sizeof(int) * nprocs);
    data->fault_conf = g_array_new(false, true, sizeof(GArray *));
    data->avoid_repeatable_faults = false;
    for(int i = 0; i < nprocs; i++){
        //data->nfaults[i] = 0;
        //data->ifaults[i] = 0;
        data->fault_conf = g_array_new(false, true, sizeof(struct sorted_fault_confs));
        struct sorted_fault_confs to_add;
        to_add.by_operation = g_array_new(false, true, sizeof(struct fault_conf));
        to_add.by_time = g_array_new(false, true, sizeof(struct fault_conf));
        to_add.itime = 0;
        to_add.ioperation = 0;
        g_array_append_val(data->fault_conf, to_add);
    }   
}

// TODO, carefull when subtrating uint64_t and then cast to gint
gint compare_fault_block_when(gconstpointer f1, gconstpointer f2){
    Fault_Conf fault1 = (Fault_Conf) f1;
    Fault_Conf fault2 = (Fault_Conf) f2;
    return (gint) fault1->when - fault2->when;
}

void add_fault_attribute(Fault_Conf_Metadata data, yaml_token_t token){
    char *lkey = data->last_key;
    char *value = (char*) token.data.scalar.value;
    //printf("key (%s) -> value (%s)\n", lkey, value);
    Fault_Conf fc = current_fault_conf(data);
    if(!strcmp(lkey, TYPE_STR)){
        if(!strcmp(value, MEDIUM_ERROR_STR)){
            fc->fault_type = FBD_FAULT_MEDIUM;
        } else if(!strcmp(value, BIT_FLIP_STR)){
            fc->fault_type = FBD_FAULT_BIT_FLIP;
        } else if(!strcmp(value, SLOW_DISK_STR)){
            fc->fault_type = FBD_FAULT_SLOW_DISK;
            fc->extra_type = FAULT_CONF_EXTRA_TIME;
        } else {
            printf("Unknown value \"%s\" for key \"%s\"", value, lkey);
        }
    } else if(!strcmp(lkey, OPERATION_STR)){
        if(!strcmp(value, WRITE_STR)){
            fc->operation = FBD_OP_WRITE;
        } else if(!strcmp(value, READ_STR)){
            fc->operation = FBD_OP_READ;
        } else if(!strcmp(value, WRITE_READ_STR)){
            fc->operation = FBD_OP_WRITE_READ;
        } else if(!strcmp(value, NONE_STR)){
            fc->operation = FBD_OP_NONE;
        } else {
            printf("Unknown value \"%s\" for key \"%s\"\n", value, lkey);
        }
    } else if(!strcmp(lkey, TARGET_STR)){
        if(!strcmp(value, LAST_BLOCK_STR)){
            fc->fault_dist = DIST_GEN;
        } else if(!strcmp(value, TOP_BLOCK_STR)){
            fc->fault_dist = TOP_DUP;
        } else if(!strcmp(value, NEXT_BLOCK_STR)){
            fc->fault_dist = NEXT_GEN;
        } else if(!strcmp(value, UNIQUE_BLOCK_STR)){
            fc->fault_dist = UNIQUE;
        } else {
            printf("Unknown value \"%s\" for key \"%s\"\n", value, lkey);
        }
    } else if(!strcmp(lkey, WHEN_OPERATION_STR)){
        fc->when = (uint64_t) atoi(value);
        fc->measure = OPS_F;
    } else if(!strcmp(lkey, WHEN_TIME_STR)){
        fc->when = (uint64_t) atoi(value);
        fc->measure = TIME_F;
    } else if(!strcmp(lkey, DELAY_STR)){
        uint32_t *delay = malloc(sizeof(uint32_t));
        *delay = atoi(value);
        fc->extra = delay;
    } else if(!strcmp(lkey, PERSISTENCE_STR)){
        if(!strcmp(value, YES_STR)){
            fc->persistent = true;
        } else if(!strcmp(value, NO_STR)){
            fc->persistent = false;
        } else {
            printf("Unkown value for persistency \"%s\": Use \"yes\" or \"no\"\n", value);
        }
    } else if(!strcmp(lkey, TRANSIENT_STR)){
        if(!strcmp(value, YES_STR)){
            fc->persistent = false;
        } else if(!strcmp(value, NO_STR)){
            fc->persistent = true;
        } else {
            printf("Unkown value for transiency \"%s\": Use \"yes\" or \"no\"\n", value);
        }
    } else if(!strcmp(lkey, MODE_STR)){
        if(!strcmp(value, MODE_HASH_STR)){
            fc->mode = FBD_MODE_HASH;
        } else if(!strcmp(value, MODE_OFFSET_STR)){
            fc->mode = FBD_MODE_BLOCK;
        } else if(!strcmp(value, MODE_DEDUP_STR)){
            fc->mode = FBD_MODE_DEDUP;
        } else if(!strcmp(value, MODE_HASH_STR)){
            fc->mode = FBD_MODE_HASH;
        } else if(!strcmp(value, MODE_DISK_STR)){
            fc->mode = FBD_MODE_DEVICE;
        }
    }
    //printf("TOKEN %s -> %s\n", data->last_key, token.data.scalar.value);
}

void handle_scalar_token(Fault_Conf_Metadata data, yaml_token_t token, struct user_confs *conf){
    char* token_str = (char*) token.data.scalar.value;
    //printf("TOKEN HERE: %s, is_key: %d, data->in_faults_sequence: %d\n", token_str, data->is_key, data->in_faults_sequence);
    if(data->is_key){
        if(data->last_key) free(data->last_key);
        data->last_key = strdup(token_str);
    } 

    if(!data->in_faults_sequence){
        if(data->is_key){
            if(!strcmp(token_str, "faults")){
                data->in_faults_scalar = true;
                //printf("data->in_faults_scalar = %d\n", data->in_faults_scalar);
                return;
            }
            /*if(!strcmp(token_str, AVOID_REPEATABLE_FAULTS_STR)){
                data->last_key = AVOID_REPEATABLE_FAULTS_STR;
                //printf("data->in_faults_scalar = %d\n", data->in_faults_scalar);
                return;
            }*/
            if(!strcmp(token_str, LENGTH_FAULTS_STR)){
                return;
            }

            if(data->key_stack_size < MAX_KEY_STACK_SIZE){
                data->key_stack[data->key_stack_size++] = strdup(token_str);
                //printf("Adding to stack %s\n", token_str);
            }
            else{
                printf("Yaml structure is too deep, exiting...\n");
                exit(0);
            } 
        } else { 
             if(!strcmp(data->last_key, AVOID_REPEATABLE_FAULTS_STR)){
                //printf("setting value for  %s repeatable faults: %s\n", AVOID_REPEATABLE_FAULTS_STR, token_str);
                if(!strcmp(token_str, YES_STR)){
                    data->avoid_repeatable_faults = true;
                } else if(!strcmp(token_str, NO_STR)){
                    data->avoid_repeatable_faults = false;
                } else {
                    printf("Unkown option for %s: %s\n", AVOID_REPEATABLE_FAULTS_STR, token_str);
                }
            }
        }
        //value is ignored
    } else {
        if(!data->is_key){
            add_fault_attribute(data, token);
        }
    }
    
    /**if(is_key){
        if(data->last_key) free(data->last_key);
        data->last_key = strdup(token.data.scalar.value);
        return;
    }**/
}

void set_is_key(Fault_Conf_Metadata data){
    data->is_key = true;
}


void set_is_value(Fault_Conf_Metadata data){
    data->is_key = false;
}

void handle_block_end(Fault_Conf_Metadata data){
    if(data->in_fault_def_block){
        //printf("ending fault...\n");
        //int *index = &data->ifaults[data->current_procid];
        Fault_Conf fc = current_fault_conf(data);
        //print_fault_conf(fc);
        if(!validate_fault_conf(fc)){
            printf("Invalid fault_conf\n");
            print_fault_conf(fc);
        }  else {
            struct sorted_fault_confs *sorted_faults = &g_array_index(data->fault_conf, struct sorted_fault_confs, data->current_procid);
            if(fc->measure == OPS_F){
                g_array_append_val(sorted_faults->by_operation, *fc);
                //printf("appending by operation: %d\n", sorted_faults->by_operation->len);
            } else if(fc->measure == TIME_F){
                //printf("appending by time\n");
                g_array_append_val(sorted_faults->by_time, *fc);
            }
        }
        data->in_fault_def_block = false;
    } else if(data->key_stack_size) {
        data->key_stack_size--;
        //printf("freeing stack key(%d): %s\n", data->key_stack_size, data->key_stack[data->key_stack_size]);
        free(data->key_stack[data->key_stack_size]);
        if(data->key_stack_size == 0){
            data->in_faults_sequence = false;
        }
    } else {
        data->in_faults_scalar = false;
    }
    
}

void handle_block_sequence(Fault_Conf_Metadata data){
    if(data->in_faults_scalar)
        data->in_faults_sequence = true;
}

/*bool check_and_increment_fault_conf_size(Fault_Conf_Metadata data){
    int *index = current_index_conf(data);
    int *size = current_size_conf(data);
    //printf("current_procid: %d, index: %d, size: %d\n", data->current_procid, *index, *size);
    if((*index)+ 1 >= (*size)){
        Fault_Conf old = data->fault_conf[data->current_procid];
        int old_size = get_fault_conf_size(data);
        data->nfaults[data->current_procid]++;
        data->fault_conf[data->current_procid] = malloc(sizeof(struct fault_conf) * data->nfaults[data->current_procid]);
        memcpy(data->fault_conf[data->current_procid], old, old_size * sizeof(struct fault_conf));
        reset_fault_conf(&data->fault_conf[data->current_procid][old_size]);
        free(old);
        return true;
    }
    return false;
}*/

int next_procid(Fault_Conf_Metadata data, int nprocs){
    int procid = nprocs > 0 ? (data->current_procid + 1) % nprocs : 0; 
    data->current_procid = procid;
    return procid;
}

void handle_block_entry(Fault_Conf_Metadata data, struct user_confs *conf){
    data->in_fault_def_block = data->in_faults_sequence;
    if(data->in_fault_def_block){
        //bool incremented = check_and_increment_fault_conf_size(data);
        Fault_Conf fc = current_fault_conf(data);
        reset_fault_conf(fc);
        //print_fault_conf(fc);
        fc->proc_id = next_procid(data, conf->nprocs);
        //printf("fc->proc_id: %d\n", fc->proc_id);
        
        for(int i = 0; i < data->key_stack_size; i++){
            char *key = data->key_stack[i]; 
            //printf("KEY: %s\n", key);
            //Operations
            if(!strcmp(key, WRITE_STR)){
                fc->operation = FBD_OP_WRITE;
            } else if(!strcmp(key, READ_STR)){
                fc->operation = FBD_OP_READ;
            } else if(!strcmp(key, WRITE_READ_STR)){
                fc->operation = FBD_OP_WRITE_READ;
            }
            //Fault-types 
            else if(!strcmp(key, MEDIUM_ERROR_STR)){
                fc->fault_type = FBD_FAULT_MEDIUM;
            } else if(!strcmp(key, BIT_FLIP_STR)){
                fc->fault_type = FBD_FAULT_BIT_FLIP;
            } else if(!strcmp(key, SLOW_DISK_STR)){
                fc->fault_type = FBD_FAULT_SLOW_DISK;
                fc->extra_type = FAULT_CONF_EXTRA_TIME_DEFAULT;
            }
            //targets
            else if(!strcmp(key, LAST_BLOCK_STR)){
                fc->fault_dist = DIST_GEN;
            } else if(!strcmp(key, TOP_BLOCK_STR)){
                fc->fault_dist = TOP_DUP;
            } else if(!strcmp(key, NEXT_BLOCK_STR)){
                fc->fault_dist = NEXT_GEN;
            } else if(!strcmp(key, UNIQUE_BLOCK_STR)){
                fc->fault_dist = UNIQUE;
            }
             //persistency
            else if(!strcmp(key, PERSISTENCE_STR)){
                fc->persistent = true;
            }
            else if(!strcmp(key, TRANSIENT_STR)){
                fc->persistent = false;
            }
            //modes
            else if(!strcmp(key, MODE_HASH_STR)){
                fc->mode = FBD_MODE_HASH;
            } else if(!strcmp(key, MODE_OFFSET_STR)){
                fc->mode = FBD_MODE_BLOCK;
            } else if(!strcmp(key, MODE_DEDUP_STR)){
                fc->mode = FBD_MODE_DEDUP;
            } else if(!strcmp(key, MODE_HASH_STR)){
                fc->mode = FBD_MODE_HASH;
            } else if(!strcmp(key, MODE_DISK_STR)){
                fc->mode = FBD_MODE_DEVICE;
            }


        }
    }
}


void end_fault_conf(Fault_Conf_Metadata data, struct user_confs* user_confs, struct faults_info* info){
    struct fault_runtime *fr = malloc(sizeof(struct fault_runtime));
    fr->last_block = malloc(sizeof(struct block_info) * user_confs->nprocs);
    fr->next_block = malloc(sizeof(struct block_info) * user_confs->nprocs);
    fr->faults = data->fault_conf;
    for(int i = 0; i < user_confs->nprocs; i++){
        struct sorted_fault_confs *sorted = &g_array_index(fr->faults, struct sorted_fault_confs, i);
        g_array_sort(sorted->by_operation, compare_fault_block_when);
        g_array_sort(sorted->by_time, compare_fault_block_when);
    }
    //fr->ifaults = data->ifaults;
    //fr->nfaults = data->nfaults;
    /*for(int i = 0; i < user_confs->nprocs; i++){
        fr->nfaults[i] = fr->ifaults[i];
        fr->ifaults[i] = 0;
    }*/
    info->faults = fr;
    info->faults->nprocs = user_confs->nprocs;
    info->faults->nunique_injected = 0;
    info->faults->avoid_repeat_faults = data->avoid_repeatable_faults;
    //printf("Faults defined, nprocs: %d:, %d\n", user_confs->nprocs, info->faults->nprocs);
    /*for(int i = 0; i < info->faults->nprocs; i++){
        struct sorted_fault_confs sorted_faults = g_array_index(info->faults->faults, struct sorted_fault_confs, i);
        printf("sizes: %d, %d\n", sorted_faults.by_operation->len, sorted_faults.by_time->len);
        for(int j = 0; j < sorted_faults.by_operation->len; j++){
            print_fault_conf(&g_array_index(sorted_faults.by_operation, struct fault_conf, j));
        }
        for(int j = 0; j < sorted_faults.by_time->len; j++){
            print_fault_conf(&g_array_index(sorted_faults.by_time, struct fault_conf, j));
        }
        Fault_Conf fc_arr = get_fault_conf_procid(data, i);
        int *index = get_fault_conf_index_of_procid(data, i);
        printf("Index_max: %d\n", *index);
        for(int j = 0; j < (*get_fault_conf_index_of_procid(data, i)); j++){
            print_fault_conf(&data->fault_conf[i][j]);
        }
    }*/
}



int parse_faults_configuration_yaml(struct user_confs* conf, char* conf_file_path, struct faults_info* info){
    FILE *conf_file = fopen(conf_file_path, "r");
    yaml_parser_t parser;
    yaml_token_t  token;
    Fault_Conf_Metadata data = malloc(sizeof(struct fault_conf_metadata));

    /* Initialize parser */
    if(!yaml_parser_initialize(&parser))
        fputs("Failed to initialize parser!\n", stderr);
    if(conf_file == NULL)
        fputs("Failed to open file!\n", stderr);

    /* Set input file */
    yaml_parser_set_input_file(&parser, conf_file);

    /* BEGIN new code */
    do {
        yaml_parser_scan(&parser, &token);
        switch(token.type) {
        /* Stream start/end */
        case YAML_STREAM_START_TOKEN: init_fault_conf_metadata(data, conf->nprocs); /*puts("STREAM START");*/ break;
        case YAML_STREAM_END_TOKEN: end_fault_conf(data, conf, info); /*puts("STREAM END");*/ break;
        /* Token types (read before actual token) */
        case YAML_KEY_TOKEN: set_is_key(data); /*printf("(Key token)   ");*/ break;
        case YAML_VALUE_TOKEN: set_is_value(data); /*printf("(Value token) ");*/ break;
        /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN: handle_block_sequence(data); /*puts("<b>Start Block (Sequence)</b>");*/ break;
        case YAML_BLOCK_ENTRY_TOKEN:          handle_block_entry(data, conf); /*puts("<b>Start Block (Entry)</b>");*/ break;
        case YAML_BLOCK_END_TOKEN:            handle_block_end(data); /*puts("<b>End block</b>");*/ break;
        /* Data */
        case YAML_BLOCK_MAPPING_START_TOKEN:  /*puts("[Block mapping]"); */ break;
        case YAML_SCALAR_TOKEN: handle_scalar_token(data, token, conf); /*printf("scalar %s \n", token.data.scalar.value);*/ break;
        /* Others */
        default:
            break; /*printf("Got token of type %d\n", token.type);*/
        }
        if(token.type != YAML_STREAM_END_TOKEN)
        yaml_token_delete(&token);
    } while(token.type != YAML_STREAM_END_TOKEN);
    yaml_token_delete(&token);
    /* END new code */

    /* Cleanup */
    yaml_parser_delete(&parser);
    fclose(conf_file);
    return 0;
}





/**
int main(int argc, char** argv){
    struct user_confs* user_confs = malloc(sizeof(struct user_confs));
    user_confs->nprocs = 1;
    return parse_faults_configuration_yaml(user_confs, "../../conf/faultconf.yaml");
}*/
