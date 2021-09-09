/* DEDISbench
 * (c) 2010 2017 INESC TEC. Written by J. Paulo
 */

#ifndef FAULT_H
#define FAULT_H

#include <glib.h>
#include <gmodule.h>

#include "../duplicates/duplicatedist.h"
#include "configParserYaml.h"
#include "dedupDegree.h"

#define DIST_GEN 0
#define TOP_DUP 1
#define BOT_DUP 2
#define UNIQUE 3
#define NEXT_GEN 4

#define TIME_F 1
#define OPS_F 2

#define TP_1 0

typedef struct faults_statistics {
    uint32_t tot_faults_injected;
    uint32_t tot_faults_dup;
    uint32_t tot_failures;
    GHashTable *injected_faults;
} *Faults_Statistics;

typedef struct fault_conf{
    int mode;
 	int measure;
 	uint64_t when;
 	int fault_dist;
 	int fault_type;
    bool persistent;
 	int proc_id;
    int operation;
    int extra_type;
    void *extra;
} *Fault_Conf;

typedef struct fault_runtime {
    GArray *faults;
    struct block_info *last_block;
    struct block_info *next_block;
    int nprocs;
    int nunique_injected;
    bool avoid_repeat_faults;
} *Fault_Runtime;

typedef struct faults_info {
    struct fault_runtime* faults;
	struct faults_statistics *fault_statistics;
} *Faults_Info;

void init_fault_statistics(struct faults_statistics *fstats);
int inject_failure(int fault_type, int fault_dist, struct duplicates_info *info, uint64_t block_size, struct user_confs* conf);
int inject_fault(Fault_Conf fault, Dedup_Degree dd, uint32_t block_size, struct faults_info *f_info, 
                  struct faults_statistics *fstats, int procid, uint64_t offset);
                  void define_failure_per_process(struct user_confs *conf);
#define decr_fault_conf_index(finfo, procid) finfo->faults->ifaults[procid] = finfo->faults->ifaults[procid]-1
Fault_Conf next_fault_conf(Fault_Runtime runtime, int procid, uint64_t n_ops, uint64_t time_elapsed);
void print_fault_conf(Fault_Conf fc);
void print_fault_statistics(FILE* out, struct faults_statistics *fstats, int procid);
void finfo_set_block_info(Faults_Info fi, struct block_info bi, int procid);
struct block_info* finfo_get_next_block(Faults_Info fi, int procid);
struct block_info* finfo_get_last_block(Faults_Info fi, int procid);;

/******************************* DUMMY VERSION *********************************/

void fault_init();
void remove_all_fauls();

/*******************************************************************************/



#endif