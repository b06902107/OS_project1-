#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>

int RR_time_last;
int now_time;

typedef struct process {
	char name[32];
	int t_ready;
	int t_exec;
	pid_t pid;
}Process;

void timer(){ 
	volatile unsigned long i;
	for(i=0 ; i<1000000UL ; i++);
}

void proc_setparent_CPU(pid_t pid){
	cpu_set_t cmask;
    unsigned long len = sizeof(cmask);
    CPU_ZERO(&cmask);   
    CPU_SET(0, &cmask);

	if(sched_setaffinity(pid, len, &cmask) < 0){
		fprintf(stderr, "parent close error with msg is: %s\n",strerror(errno));
   		exit(1);
 	}
}

void proc_setchild_CPU(pid_t pid){
	cpu_set_t cmask;
    unsigned long len = sizeof(cmask);
    CPU_ZERO(&cmask);   
    CPU_SET(1, &cmask);

	if(sched_setaffinity(pid, len, &cmask) < 0){
		fprintf(stderr, "child close error with msg is: %s\n",strerror(errno));
   		exit(1);
 	}
}

void proc_wakeup(pid_t pid){
	struct sched_param param;
	param.sched_priority = 0;

	if(sched_setscheduler(pid, SCHED_OTHER, &param) < 0){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
}

void proc_block(int pid){
	struct sched_param param;
	param.sched_priority = 0;
	
	if(sched_setscheduler(pid, SCHED_IDLE, &param) < 0){
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
}

pid_t proc_exec(Process proc){
	int pid = fork();
	if(pid == 0){
		unsigned long start_sec, start_nsec, end_sec, end_nsec;
		char to_dmesg[256];
		syscall(334, &start_sec, &start_nsec);
		for(int i=0 ; i<proc.t_exec ; i++)
			timer();
		syscall(334, &end_sec, &end_nsec);
		sprintf(to_dmesg, "[Project1] %d %lu.%09lu %lu.%09lu\n", getpid(), start_sec, start_nsec, end_sec, end_nsec);
		//printf("%s", to_dmesg);
		syscall(335, to_dmesg);
		exit(0);
	}
	
	proc_setchild_CPU(pid);
	return pid;
}

int choose_next_process(Process *proc, int process_num, int policy, int running){
	int ret = -1;
	if(policy == 0){
		if(running != -1)
			return running;

		for(int i=0 ; i<process_num ; i++){
			if(proc[i].pid == -1)
				continue;
			if(proc[i].t_exec == 0)
				continue;
			if(ret == -1 || proc[i].t_ready < proc[ret].t_ready)
				ret = i;
		}
		return ret;
	}
	else if(policy == 1){
		if(running == -1){
			for(int i=0 ; i<process_num ; i++){
				if(proc[i].pid != -1 && proc[i].t_exec > 0){
					ret = i;
					break;
				}
			}
		}
		else if((now_time - RR_time_last) % 500 == 0){
			if(running != (process_num-1))
				ret = running + 1;
			else
				ret = 0;
			while(proc[ret].pid == -1 || proc[ret].t_exec == 0){
				if(ret != (process_num-1))
					ret = ret + 1;
				else
					ret = 0;
			}
		}
		else{
			ret == running;
		}
		return ret;
	}
	else if(policy == 2){
		if(running != -1)
			return running;

		for(int i=0 ; i<process_num ; i++){
			if(proc[i].pid == -1)
				continue;
			if(proc[i].t_exec == 0)
				continue;
			if(ret == -1 || proc[i].t_exec < proc[ret].t_exec)
				ret = i;
		}
		return ret;
	}
	else if(policy == 3){
		for(int i=0 ; i<process_num ; i++){
			if(proc[i].pid == -1)
				continue;
			if(proc[i].t_exec == 0)
				continue;
			if(ret == -1 || proc[i].t_exec < proc[ret].t_exec)
				ret = i;
		}
		return ret;
	}
	else{
		fprintf(stderr, "Invalid policy");
		exit(0);
	}

	return ret;
}

void scheduling(Process *proc, int process_num, int policy){
	proc_setparent_CPU(getpid());

	proc_wakeup(getpid());

	now_time = 0;
	int running_proc = -1;
	int finish_proc_num = 0;

	while(1){
		if(running_proc != -1 && proc[running_proc].t_exec == 0){
			waitpid(proc[running_proc].pid, NULL, 0);
			printf("%s %d\n", proc[running_proc].name, proc[running_proc].pid);
			fflush(stdout);
			running_proc = -1;

			finish_proc_num++;
			if(finish_proc_num == process_num)
				break;
		}

		for(int i=0 ; i<process_num ; i++){
			if(proc[i].t_ready == now_time){
				proc[i].pid = proc_exec(proc[i]);
				proc_block(proc[i].pid);
			}
		}

		int next_proc = choose_next_process(proc, process_num, policy, running_proc);
		if(next_proc != -1){
			if (running_proc != next_proc){
				proc_wakeup(proc[next_proc].pid);
				proc_block(proc[running_proc].pid);
				running_proc = next_proc;
				RR_time_last = now_time;
			}
		}

		timer();
		if(running_proc != -1)
			proc[running_proc].t_exec--;
		now_time++;
	}
}

int main(int argc, char *argv[]){

	char policy_c[256];
	int policy;
	int process_num;

	scanf("%s", policy_c);
	scanf("%d", &process_num);

	Process proc[process_num];
	for(int i=0 ; i<process_num ; i++){
		scanf("%s %d %d", proc[i].name, &proc[i].t_ready, &proc[i].t_exec);
		proc[i].pid = -1;
	}

	if(strcmp(policy_c, "FIFO") == 0)
		policy = 0;
	else if(strcmp(policy_c, "RR") == 0)
		policy = 1;
	else if(strcmp(policy_c, "SJF") == 0)
		policy = 2;
	else if(strcmp(policy_c, "PSJF") == 0)
		policy = 3;

	scheduling(proc, process_num, policy);

	exit(0);
}
