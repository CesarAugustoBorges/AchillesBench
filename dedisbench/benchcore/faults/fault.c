/* DEDISbench
 * (c) 2010 2017 INESC TEC. Written by J. Paulo
 */

#include "fault.h"
#include "../../utils/random/random.h"
#include <string.h>
#include <stdlib.h>

#include <fsp_client.h>
#include <fbd_defines.h>

#include "../duplicates/duplicatedist.h"

int socket;
int socket_inited;

void fault_init(){
  socket = fsp_socket();
  if(socket >= 0){
    fsp_connect(socket);
    socket_inited = 1;
  } else {
    socket_inited = -1;
  }
}

void init_fault_statistics(struct faults_statistics *fstats){
  fstats->injected_faults = g_hash_table_new(g_int64_hash, g_int64_equal); 
}

int next_failure(char* buf, int fault_dist, Dedup_Degree dd, uint64_t block_size, 
                struct faults_info *f_info, struct faults_statistics *fstats, int procid, struct block_info **bl ){
  uint32_t index = 0;
  switch(fault_dist){
    case DIST_GEN:
      *bl = finfo_get_last_block(f_info, procid);
      //printf("DIST_GEN Injecting failure for content %llu procid %d\n", (unsigned long long int)bl.cont_id, bl.procid);
      //printf("cont id %lu, procid %d, ts: %lu\n", bl.cont_id, bl.procid, bl.ts);
      get_block_content(buf, **bl, block_size);
    break;
    case NEXT_GEN:
      *bl = finfo_get_next_block(f_info, procid);
      //printf("NEXT_BLOCK Injecting failure for content %lu procid %d\n", (*bl)->cont_id, (*bl)->procid);
      get_block_content(buf, **bl, block_size);
      break;
    case TOP_DUP:
      //printf("avoid faults? %s\n", f_info->faults->avoid_repeat_faults ? "Yes" : "No");
      if(!f_info->faults->avoid_repeat_faults){
        *bl = get_nth_more_duplicate(dd, index);
      } else {
        *bl = get_nth_more_duplicate(dd, index);
        gboolean found = g_hash_table_contains(fstats->injected_faults, &(*bl)->cont_id);
        while(found && dd->dedup_writes.sorted_array->len > index){
          index++;
	 // printf("found :%d, cont_id: %lu\n", found, (*bl)->cont_id);

          *bl = get_nth_more_duplicate(dd, index);
          found = g_hash_table_contains(fstats->injected_faults, &(*bl)->cont_id
);
        }
      }
      //printf("TOP_DUP Injecting failure for content %llu\n", (unsigned long long int)bl);
      get_block_content(buf, **bl, block_size);
      //printf("index %d\n", index);
      break;
    case BOT_DUP:
      *bl = get_nth_least_duplicate(dd);
      //printf("BOT_DUP Injecting failure for content %llu\n", (unsigned long long int)info->botblock);
      get_block_content(buf, **bl, block_size);
      break;
    case UNIQUE:
      if(dd->unique_blocks_writes->len >= f_info->faults->nunique_injected){
        index = f_info->faults->nunique_injected;
      }
      *bl = get_nth_unique(dd, index);
      printf("UNIQUE_ Injecting failure for content %lu\n", (*bl)->cont_id);
      get_block_content(buf, **bl, block_size);
      f_info->faults->nunique_injected++;
      break;
    default:
      perror("Unknown fault distribution\n");
      exit(0);
      break;
  }


  return 0;
}

int inject_fault(Fault_Conf fault, Dedup_Degree dd, uint32_t block_size, struct faults_info *f_info, 
                  struct faults_statistics *fstats, int procid, uint64_t offset){
  
  char block[4096] = {0x61};
  struct block_info *bl_ptr;
  struct block_info **bl = &bl_ptr;
  //if(fault->mode != FBD_MODE_BLOCK && fault->mode != FBD_MODE_DEVICE){
  next_failure(block, fault->fault_dist, dd, block_size, f_info, fstats, procid, bl);
  //}
  //printf("header(%lu):\"%s\"\n", block_size, header);
  //get_block_content(block, info, block_size);
  //printf("(%d)block: %s\n", block_size, block);
  
  void* fault_extra = NULL;
  int fault_extra_size = 0;
  if(fault->fault_type == FBD_FAULT_SLOW_DISK){
    fault_extra = fault->extra;
    fault_extra_size = sizeof(uint32_t);
  }
  int res; 
  switch(fault->mode){
    case FBD_MODE_HASH:
      res = fsp_add_generic_hash(socket, block_size, block, fault->operation, fault->fault_type, 
                                              fault->persistent, FBD_HASH_XXH3_128, 
                                              fault_extra, fault_extra_size);
      break;
    case FBD_MODE_BLOCK:
      res = fsp_add_generic_block(socket, block_size, offset, fault->operation,
                                              fault->fault_type, fault->persistent, 
                                              fault_extra, fault_extra_size);
      break;
    case FBD_MODE_DEDUP:
      res = fsp_add_generic_dedup(socket, block_size, block, fault->operation, fault->fault_type, 
                                              fault->persistent, FBD_HASH_XXH3_128, 
                                              fault_extra, fault_extra_size);
      break;
    case FBD_MODE_DEVICE:
      res = FBD_STS_ERROR;
      break;
    default:
      res = FBD_STS_ERROR;
  }
  
  //char * res_str = fbd_response_to_string(res);
  //printf("response: %d\n", res);
  if(res > 0 && res != FBD_STS_DUP_FAULT){
    //printf("tot faults: %u, %p\n", fstats->tot_faults_injected, &(fstats));
    //printf("adding cont_id:%lu to hash table\n", (*bl)->cont_id);
    g_hash_table_add(fstats->injected_faults, &(*bl)->cont_id);
    fstats->tot_faults_injected++;
  }
  if(res > 0 && res == FBD_STS_DUP_FAULT){
    fstats->tot_faults_dup++;
  }
  /*if(res <= 0){
    printf("RESPONSE ERROR: %s\n", res_str);
  } else {
    printf("RESPONSE: %s\n", res_str );
  }*/
  return 0;
}


//Aux function to split the string with multiple faults
int fault_split(char* a_str, struct user_confs *conf){
    char* tmp = a_str;
    int nr_faults=0;
   
    //starts with one because one element does not need the comma
    while (*tmp)
    {
        if (',' == *tmp)
        {
            nr_faults++;
        }
        tmp++;
    }
    nr_faults++;

    conf->fconf = malloc(sizeof(struct fault_conf)*nr_faults);

    char* token = strtok(a_str, ",");

    int i=0;
    while (token)
    {
        char type[20];
        char when[20];
        char dist[20];
        int sep=0;
        int iter=0;
        char *info=strdup(token);

        while(*info){
          if(*info == ':'){
            sep++;
            iter=0;
          }else{
            switch(sep){
              case 0:
                type[iter]=*info;
                break;
              case 1:
                dist[iter]=*info;
              default:
                when[iter]=*info;
                break;
            }
            iter++;
          }
          info++;
        }

        if(atoi(when)==0){
          printf("Cannot inject a failure at time/nr operations == 0\n");
          return -1;
        }

        if(atoi(dist)==DIST_GEN && conf->iotype==READ){
          printf("Failure type following content generation distribution is not supported for single read tests\n");
          return -1;
        }

        conf->fconf[i].fault_type=atoi(type);
        conf->fconf[i].fault_dist=atoi(dist);
        conf->fconf[i].when=atoi(when);
        
        printf("Arg is type %d, dist %d, when %lu\n",conf->fconf[i].fault_type, conf->fconf[i].fault_dist, conf->fconf[i].when);
        token = strtok(NULL, ",");
        i++;
        
    }


    return nr_faults;

}


	void define_failure_per_process(struct user_confs *conf){
	int i;

	for(i =0; i<conf->nr_faults;i++){
		conf->fconf[i].proc_id=genrand(conf->nr_proc_w);
	}
}

Fault_Conf next_fault_conf(Fault_Runtime runtime, int procid, uint64_t n_ops, uint64_t time_elapsed){
    //printf("n_ops: %lu, time: %lu\n", n_ops, time_elapsed);
    if(!runtime) 
      return NULL;
    struct sorted_fault_confs *sorted_faults = &g_array_index(runtime->faults, struct sorted_fault_confs, procid);
    Fault_Conf res = NULL;
    //printf("itime: %u, len: %d, ioperation: %u, len: %d\n", sorted_faults->itime, sorted_faults->by_time->len, sorted_faults->ioperation, sorted_faults->by_operation->len);
    if(sorted_faults->itime < sorted_faults->by_time->len){
      Fault_Conf fc = &g_array_index(sorted_faults->by_time, struct fault_conf, sorted_faults->itime);
      if(fc->when <= time_elapsed){
        res = fc;
        sorted_faults->itime ++;
      }
    } else if(sorted_faults->ioperation < sorted_faults->by_operation->len){
      Fault_Conf fc = &g_array_index(sorted_faults->by_operation, struct fault_conf, sorted_faults->ioperation);
      if(fc->when <= n_ops){
        //printf("when: %lu, op %lu\n", fc->when, n_ops);
        res = fc;
        sorted_faults->ioperation ++;
      }
    }
    /*if(res){
      print_fault_conf(res);
    }*/
    return res;
    /*
    printf("n_ops: %lu, time: %lu\n", n_ops, time_elapsed);
    if(runtime->ifaults[procid] < runtime->nfaults[procid]){
        int fault_index = runtime->ifaults[procid];
        Fault_Conf res = &runtime->faults[procid][fault_index];
        if(res->measure == TIME_F && res->when >= time_elapsed){
          return NULL;
        } else if(res->measure == OPS_F && res->when >= n_ops){
          return NULL;
        }
        runtime->ifaults[procid] = runtime->ifaults[procid] + 1;
        return res;
    }*/ 
}

void print_fault_conf(Fault_Conf fc){
    printf("--------------- Fault Conf --------------\n");
    printf("fault_type: %d, fault_dist: %d\n", fc->fault_type, fc->fault_dist);
    printf("measure: %d, when: %lu\n", fc->measure, fc->when);
    printf("operation: %d, procid: %d\n", fc->operation, fc->proc_id);
    printf("persistent? %s\n", fc->persistent ? "Yes" : "No");
    if(fc->extra_type == FAULT_CONF_EXTRA_TIME){
        printf("delay: %d\n", *((uint32_t *) fc->extra));
    }
    printf("-----------------------------------------\n");
}

void print_fault_statistics(FILE* out, struct faults_statistics *fstats,int procid){
  if(fstats){
    fprintf(out, "Number of faults injected: %u\n", fstats->tot_faults_injected);
    GList *injected = g_hash_table_get_keys(fstats->injected_faults);
    while(injected){
      uint64_t bi = (uint64_t) injected->data;
      fprintf(out, "ContentId: %lu\n", bi);
      injected = injected->next;
    }
    fprintf(out, "Number of ignored faults: %d\n", fstats->tot_faults_dup);
    fprintf(out, "Number of failures: %u\n", fstats->tot_failures);
  }
}

void finfo_set_block_info(Faults_Info fi, struct block_info bi, int procid){
  if(fi && fi->faults){
    fi->faults->last_block[procid] = fi->faults->next_block[procid];
    fi->faults->next_block[procid] = bi;
  }
}


struct block_info* finfo_get_next_block(Faults_Info fi, int procid){
  if(fi && fi->faults){
    return &fi->faults->next_block[procid];
  }
  return NULL;
}


struct block_info* finfo_get_last_block(Faults_Info fi, int procid){
  if(fi && fi->faults){
    return &fi->faults->last_block[procid];
  }
  return NULL;
}

void remove_all_fauls(){
  fsp_remove_all_faults(socket);
}

/*int main(int argc, char** argv){
  struct faults_info  finfo= {.fault_statistics = {.tot_failures = 0, .tot_faults_injected = 10}};
  FILE* out = fopen("./test", "w");
  print_fault_statistics(out, &finfo);
}*/
