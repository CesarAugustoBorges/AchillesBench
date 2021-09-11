#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

void start_pidstat(int *pid, char* output){
	*pid = fork();
	if(*pid == 0){
		int p[2];
		if(pipe(p) < 0){
			exit(1);
		}
		if(!fork()){
			FILE *fd = fopen(output, "w");
			if(fd < 0) exit(1);
			close(p[1]);
			int r;
			char buf[513];
			buf[512] = '\0';
			int stop = 0;
			while(!stop){
				r = read(p[0], buf, 512);
				buf[r] = '\0';
				fprintf(fd, "%s", buf);
				stop = r <= 0;
			}
			fclose(fd);
			close(p[0]);
			exit(0);
		} else {
			close(p[0]);
			dup2(p[1], 1);
			dup2(p[1], 2);
			//close(fd);

			char *params[] = {"/usr/bin/pidstat","--human", "-l", "-r", "-u", "-h", "-H", "-d", "-v", "-w", "-G", "^./DEDISbench", "1", NULL};
			int error = execv(params[0], params);
			if(error){
				perror("ERROR with execvp \"pidstat\"");
			}
			exit(1);
		}
	}
}

void start_dedisbench(int* pid, char* output, char* conf_file){
    *pid = fork();
	if(*pid == 0){
		int p[2];
		if(pipe(p) < 0){
			exit(1);
		}
		if(!fork()){
			FILE *fd = fopen(output, "w");
			if(fd < 0) exit(1);
			close(p[1]);
			int r;
			char buf[513];
			buf[512] = '\0';
			int stop = 0;
			while(!stop){
				r = read(p[0], buf, 512);
				buf[r] = '\0';
				fprintf(fd, "%s", buf);
				stop = r <= 0;
			}
			fclose(fd);
			close(p[0]);
			exit(0);
		} else {
			close(p[0]);
			dup2(p[1], 1);
			dup2(p[1], 2);
			//close(fd);
			// "-s65536" "-t20"
			char conf_file_opt[128] = "-f";
			strcat(conf_file_opt, conf_file);
			char *params[] = {"./DEDISbench","-p", "-w", "-s65536", conf_file_opt, NULL};
            int error = execv(params[0], params);
            if(error){
                perror("ERROR with execvp \"dedisbench\"");
            }
            exit(1);
		}
	}
}

void stop_pidstat(int *pid){
	if(*pid == 0){
		printf("No pid to kill\n");
		return;
	}
	kill(*pid, SIGTERM);
	int status;
	if(wait(&status) < 0){
		perror("error waiting for pidstat");
	}
}


int main(int argc, char**argv){
    if(argc != 4){
        printf("use \"dedis_pidstat <dedisbench_output> <pidstat_output> <dedis_conf_file>\"\n");
        return 0;
    }
    int dPid, pPid, status, rPid;
    printf("Starting pidstat\n");
    start_pidstat(&pPid, argv[2]);
    printf("Starting dedisbench\n");
    start_dedisbench(&dPid, argv[1], argv[3]);
    printf("waiting for dedis\n");
    rPid = wait(&status);

    if(status && rPid == pPid){
        //There was an error in pidstat, wait for dedisbench and exit
        wait(&status);
        return 1;
    }
    stop_pidstat(&pPid);
    return 0;
}
