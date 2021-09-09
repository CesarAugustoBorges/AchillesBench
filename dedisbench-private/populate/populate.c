/* DEDISbench
 * (c) 2010 2017 INESC TEC and U. Minho
 * Written by J. Paulo
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <sys/types.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "../utils/random/random.h"
#include "populate.h"
#include "../benchcore/faults/dedupDegree.h"



int open_rawdev(char* rawpath, struct user_confs *conf){

  int fd_test;
  if(conf->odirectf==1){
     fd_test = open(rawpath, O_RDWR | O_LARGEFILE | O_DIRECT, 0644);
    if(fd_test==-1) {
      perror("Error opening file for process I/O O_DIRECT");
       exit(0);
    }
  }
  else{
    fd_test = open(rawpath, O_RDWR | O_LARGEFILE, 0644);
    if(fd_test==-1) {
      perror("Error opening file for process I/O");
       exit(0);
    }
  }
  return fd_test;
}

//we must check this
//create the file where the process will perform I/O operations
int create_pfile(int procid, struct user_confs *conf){

  int fd_test;

  //create the file with unique name for process with id procid
  char name[PATH_SIZE];
  char id[4];
  sprintf(id,"%d",procid);
  strcpy(name,conf->tempfilespath);
  strcat(name,TMP_FILE);
  strcat(name,id);
  
  //printf("%s\n", conf->tempfilespath); 
  if(conf->odirectf==1){
    //printf("opening %s with O_DIRECT\n",name);
    //device where the process will write
    fd_test = open(name, O_RDWR | O_LARGEFILE | O_CREAT | O_DIRECT, 0644);
  }
   else{
     //printf("opening %s\n",name);
     //device where the process will write
     fd_test = open(name, O_RDWR | O_LARGEFILE | O_CREAT, 0644);
   }
   if(fd_test==-1) {
     perror("Error opening file for process I/O");
     exit(0);
   }

   return fd_test;
}




int destroy_pfile(int procid, struct user_confs *conf){

  //create the file with unique name for process with id procid
  char name[PATH_SIZE];
  char id[4];
  sprintf(id,"%d",procid);
  strcpy(name,"rm ");
  strcat(name,conf->tempfilespath);
  strcat(name,TMP_FILE);
  strcat(name,id);

  printf("performing %s\n",name);

  int ret = system(name);
  if(ret<0){
      perror("System rm failed");
  }

  return 0;
}

uint64_t dd_populate(char* name, struct user_confs* conf){

  //create the file with unique name for process with id procid
  char command[PATH_SIZE];
  char count[10];
  sprintf(count,"%llu",(long long unsigned int)conf->filesize/1024/1024);  
  strcpy(command,"dd if=/dev/zero of=");
  strcat(command, name);
  strcat(command," bs=1M count=");
  strcat(command, count);

  printf("populating file/device %s with %s\n",name,command);
  int ret = system(command);
  if(ret<0){
    perror("System dd failed");
  }

  return conf->filesize;

}



uint64_t real_populate(int fd, struct user_confs *conf, struct duplicates_info *info, int idproc){

  struct stats stat;

  //init random generator
  //if the seed is always the same the generator generates the same numbers
  //for each proces the seed = seed + processid or all the processes would
  //here is seed+nrprocesses so that in the population the load is different
  //generate the same load
  init_rand(conf->seed+conf->nprocs);

  
  uint64_t bytes_written=0;
  while(bytes_written<conf->filesize){

    char* buf;
    struct block_info info_write;

    //memory block
    if(conf->odirectf==1){
      buf = memalign(conf->block_size,conf->block_size);
    }else{
      buf = malloc(conf->block_size);
    }



    get_writecontent(buf, conf, info, &stat, 0, &info_write);


    if(conf->distout==1 || conf->fault_measure>0){
      uint64_t idwrite=info_write.cont_id;

      if(idwrite<info->duplicated_blocks){
        info->statistics[idwrite]++;
        if(info->statistics[idwrite]>1){
           if(info->statistics[idwrite]>info->topblock_dups){
              info->topblock=idwrite;
              info->topblock_dups=info->statistics[idwrite];
           }
           if(info->statistics[idwrite]<info->botblock_dups){
              info->botblock=idwrite;
              info->botblock_dups=info->statistics[idwrite];
           }
        }
        else{
          if(info->statistics[idwrite]<=1){
            stat.uni++;
          }
        }
      }
    }
    int res = pwrite(fd,buf,conf->block_size,bytes_written);
    if(res<conf->block_size){
      perror("Error populating file");
    }

    if(conf->integrity>=1){
          int pos = (conf->rawdevice==1) ? 0 : idproc;
          info->content_tracker[pos][bytes_written/conf->block_size].cont_id=info_write.cont_id;     
          info->content_tracker[pos][bytes_written/conf->block_size].procid=info_write.procid;
          info->content_tracker[pos][bytes_written/conf->block_size].ts=info_write.ts;
    }

    free(buf);
    
    bytes_written+=conf->block_size;
  }

  return bytes_written;
  

}



//populate files with content
void populate(struct user_confs *conf, struct duplicates_info *info){

  int i;
  int fd;
  int nprocs=0;

  if(conf->mixedIO==1){
  	// If running mixed test with only one proc this will zero
	// and it wont start the populate
    nprocs=conf->nprocs/2;
  }
  else{
    nprocs=conf->nprocs;
  }

  uint64_t bytes_populated=0;

  if(conf->rawdevice==0){

    //for each process populate its file with size filesize
    //we use DD for filling a non sparse image
	//
	//(!conf->mixedIO && i<nprocs) || (conf->mixedIO && i<nprocs)
    for(i=0;i < nprocs ;i++){
        //create the file with unique name for process with id procid
        char name[PATH_SIZE];
        char id[4];
        sprintf(id,"%d",i);
        strcpy(name,conf->tempfilespath);
        strcat(name,TMP_FILE);
        strcat(name,id);
          
        if(conf->populate==DDPOP){

          bytes_populated += dd_populate(name, conf);

        }else{
          

          printf("populating file %s with realistic content\n",name);

          fd = create_pfile(i,conf);
          bytes_populated += real_populate(fd, conf, info, i);  
          fsync(fd);     
          close(fd);
	  	  
        }


    }
  }  
  else{

    if(conf->populate==DDPOP){

      bytes_populated += dd_populate(conf->rawpath, conf);


    }else{

       
      printf("populating device %s with realistic content\n",conf->rawpath);

      fd = open_rawdev(conf->rawpath,conf);
      bytes_populated += real_populate(fd, conf, info, 0);
      fsync(fd);
      close(fd);

    }


  }

  printf("File/device(s) population is completed wrote %llu bytes\n", (unsigned long long int)bytes_populated);

}


//DEAD CODE?
void insert_bug(int fd, int block_size, int offset){

  char bug_buffer[block_size+1];
  int i = 0;

  /* buffer full of 'b's */
  for(i=0; i < block_size;i++) bug_buffer[i]='b';
  pwrite(fd, bug_buffer, block_size, offset);

}

int file_integrity(int fd, struct user_confs *conf, struct duplicates_info *info, int idproc, FILE* fpi){

  char *buf;
  int res=0;


  //memory block
  if(conf->odirectf==1){
    buf = memalign(conf->block_size,conf->block_size);
  }else{
    buf = malloc(conf->block_size);
  }

  uint64_t bytes_read=0;
  while(bytes_read<conf->filesize){

    struct block_info bf = info->content_tracker[idproc][bytes_read/conf->block_size];
    //Block was written before
    if(bf.ts && bf.cont_id){
      int res_pread = pread(fd,buf,conf->block_size,bytes_read);
      //printf("read (n: %d, o: %d): %s\n", res_pread, bytes_read, buf);
      //printf("pread size %lu  offset %lu res %d...\n", conf->block_size, bytes_read, res);
      if(res_pread<=0){
        //perror("Error reading in integrity tests\n");
        printf("Reading block in offset %llu, with size %lu returned %d. Maybe the files was not populated correctly?\n", bytes_read, conf->block_size, res_pread);
        fprintf(fpi, "Reading block in offset %lu, with size %lu returned %d. Maybe the files was not populated correctly? The next integrity error is related with this message: ", bytes_read, conf->block_size, res_pread);   
      }
      res+=compare_blocks(buf, bf, conf->block_size, fpi, 1);
    }
    bytes_read+=conf->block_size;
  }

  free(buf);

  return res;


}


int check_integrity(struct user_confs *conf, struct duplicates_info *info){

  int i;
  int fd;
  int nprocs=0;

  FILE *fpi=NULL;
  int integrity_errors=0;
  char ifilename[PATH_SIZE];
  strcpy(ifilename,"./results/intgr_final_static_check");
  fpi=fopen(ifilename,"w");
  fprintf(fpi,"Final Integrity Check results\n");

  if(conf->mixedIO==1){
    nprocs=conf->nprocs/2;
  }
  else{
    nprocs=conf->nprocs;
  }

  printf("File/device(s) integrity check is now Running...\n");

  if(conf->rawdevice==0){

    //for each process populate its file with size filesize
    //we use DD for filling a non sparse image
    for(i=0;i<nprocs;i++){
        //create the file with unique name for process with id procid
        char name[PATH_SIZE];
        char id[4];
        sprintf(id,"%d",i);
        strcpy(name,conf->tempfilespath);
        strcat(name,TMP_FILE);
        strcat(name,id);
        
        printf("Running for proc %s...\n", name);

        fd = create_pfile(i,conf);
        integrity_errors += file_integrity(fd,conf, info, i, fpi);
        close(fd);        
    }
  }  
  else{
    fd = open_rawdev(conf->rawpath,conf);
    integrity_errors += file_integrity(fd,conf, info, 0, fpi);
    close(fd);
  }

  if(integrity_errors>0){
    printf("Found %d integrity errors see %s file for more details\n", integrity_errors, ifilename);
    fprintf(fpi,"Found %d integrity errors\n", integrity_errors);
  }else{
    fprintf(fpi,"No integrity issues found\n");
    printf("No integrity issues found\n");
  }
  fclose(fpi);

  printf("File/device(s) integrity check is completed\n");

  return integrity_errors;
}
