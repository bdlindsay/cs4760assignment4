#include "oss.h"

// Brett Lindsay
// cs4760 assignment4
// oss.c

char *arg2; // to send process_id num to process
char *arg3; // to send pcb shm_id num to process 
char *arg4; // to send runInfo shm_id num to process 
char *arg5; // to send lClock shm_id to process
pcb_t *pcbs[18] = { NULL };
run_info_t *runInfo;
int num = 0;
//bool timed_out = false;
// signal handler prototypes
void free_mem();
void timeout();

main (int argc, char *argv[]) {
	char *arg1 = "userProcess";
	arg2 = malloc(sizeof(int)); // process num in pcbs
	arg3 = malloc(sizeof(int)); // shm_id to pcb
	arg4 = malloc(sizeof(int)); // shm_id to runInfo
	int i, shm_id, q;
	double r; // for random "milli seconds"
	int usedPcbs[18] = { 0 }; // "bit vector" for used PIDs
	bool isPcbsFull = false;
	int next, nextCreate = 0; // points to next available PID
	srandom(time(NULL));
	signal(SIGINT, free_mem);
	signal(SIGALRM, timeout);
	//alarm(60);	

	// create shared runInfo
	if((shm_id = shmget(IPC_PRIVATE,sizeof(run_info_t*),IPC_CREAT|0755)) == -1){
		perror("shmget:runinfo");
	}	
	runInfo = (run_info_t*) shmat(shm_id,0,0);
	runInfo->shm_id = shm_id;
	runInfo->lClock = 0.000;
	// set burst and process_num each time a process is chosen
	//q = -1; // 1 queue b/w process creation, init -1 so first queue is 0
	while(1) { // infinite loop until alarm finishes
		if (runInfo->lClock > 120) {
			alarm(1);
		}
		if (nextCreate < runInfo->lClock) {
			isPcbsFull = true;
			for (i = 0; i < 18; i++) {
				if (usedPcbs[i] == 0) {
					next = i;	
					isPcbsFull = false;	// need to set false when process killed
					break;
				}	
			}
			if (isPcbsFull) {
				fprintf(stderr, "pcbs array was full trying again...\n");
				// update logical clock if pcbs is full
				r = (double)(random() % 1000) / 1000;	
				updateClock(r);
				continue;
			}
			// create and assign pcb to the OS's array
			// init PCB
			fprintf(stderr, "Creating new process at pcb[%d]\n",next);
			pcbs[next] = initPcb();
			usedPcbs[next] = true;

			// next process creation time
			r = rand() % 3; // 0-2
			nextCreate = runInfo->lClock + r;
		} // end create process if block
		
		// schedule a process
		// process create -> all queues -> process create...
		for (i = 0; i < 3; i++) {
			runInfo->burst = 3 - i; // -1 sec burst for each lower priority
	  	scheduleProcess(i, arg1, -1, -1); // -1 is NULL value for func
		}
		/*// process create -> queue -> process create...
		runInfo->burst = 3 - q; // -1 sec burst for each lower priority
		scheduleProcess(q, arg1, -1, -1); // -1 is NULL value for func*/

		// check for finished processes
		updatePcbs(usedPcbs);

		// update logical clock
		r = (double)(random() % 1000) / 1000;	
		updateClock(r);
		/*if (++q >= 3) // proc create -> queue -> proc create 0 -> 1 -> 2 -> 0... 
			q = 0;*/	
	} // end infinite while	

	// cleanup after normal execution - never reached in current implementation
	cleanUp(); // clean up with free(), remove lClock, call cleanUpPcbs()
}

// check for finished processes
void updatePcbs(int usedPcbs[]) {
	int i;
	num = 0; // number of priority 2 processes
	for (i = 0; i < 18; i++) {
		if (pcbs[i] == NULL) {
			continue;
		}	
	/*	fprintf(stderr, "oss: Checking if process %d is done: %d\n",i, 
			(bool)pcbs[i]->isCompleted);*/
		if (pcbs[i]->isCompleted) {
			// TODO collect data on userProcess
			fprintf(stderr,"oss: Removing finished pcb[%d]\n",i);
			removePcb(pcbs, i);
			usedPcbs[i] = false;
		} else if (pcbs[i]->ioInterupt) {
			fprintf(stderr, "Process %d seems io-bound, maintain priority.\n", i);	
		}	else if (pcbs[i]->priority == 0) { // if not completed, no io intr
			fprintf(stderr, "Process %d decreased to priority 1.\n",i);
			pcbs[i]->priority = 1;
		} else if (pcbs[i]->priority == 1) {
			fprintf(stderr, "Process %d decreased to priority 2.\n",i);
			pcbs[i]->priority = 2;
			num++;
		}
	} 
} // end updatePcbs()

double calcCompletionTime(int i) {
	// SRTF
	// total time to complete - total cpu time so far = completion time
	return (double) pcbs[i]->timeToComplete - pcbs[i]->totalCpuTime;
}
double getCompletionTime(int i) {
	// SJN
	return pcbs[i]->timeToComplete;
}

// which process should run next, queue 0 or 1 SJN, queue 2 Round Robin (rr)
// send scheduleProcess queue -1 and a valid process number
// for rr_p_num schedules that process to run for runInfo->burst/burstDiv
void scheduleProcess(int queue, char *arg1, int rr_p_num, int burstDiv) {
	int selected = -1;

	// schedule next process
	if (queue == 0 || queue == 1) {
		selected = findSJN(queue);	
	} else if (queue == 2) {
		scheduleRR(queue, arg1);	
	} else if (queue == -1) {
		runInfo->burst /= burstDiv;
		selected = rr_p_num;
	}
	// in case there are no processes ready
	if (selected == -1) {
		fprintf(stderr,"Queue %d: No available processes to schedule\n", queue);
		return;
	}	
	
	// tell selected process to run
	if(pcbs[selected] != NULL && !pcbs[selected]->isCompleted) { //safety
		// -1 value of queue is really queue 2
		if (queue == -1) {
			queue = 2;
		}
		fprintf(stderr,"Queue %d scheduling process %d.\n", queue, selected);
		// args for userProcess
		sprintf(arg2, "%d", selected); // process num in pcbs
		sprintf(arg3, "%d", pcbs[selected]->shm_id); // pcb for userProcess
		sprintf(arg4, "%d", runInfo->shm_id); // runInfo for userProcess

		if ((pcbs[selected]->pid = fork()) == -1) {
			perror("fork");
			return; // if fork() fails don't exec
		}
		if (pcbs[selected]->pid == 0) { // child process 
			execl("userProcess", arg1, arg2, arg3, arg4, 0);
		} 
		sem_signal(pcbs[selected]->sem_id,0); 
		wait();
		pcbs[selected]->pid = -1; // process technically ended
	}
} // end scheduleProcess()

// determine shortest process for SJN strategey priority 0,1
int findSJN(int queue) {
	int k;
	int selected = -1;
	double sjn = 0; // shortest job next
	double tmp = 0; // holder
	
	// find shortest out of processes in queue
	for (k = 0; k < 18; k++) {
		// continue if no pcb for this index
		if(pcbs[k] != NULL && pcbs[k]->priority == queue &&
			 !pcbs[k]->isCompleted) { 
			if (sjn == 0) { // init to completion time of first non-null pcb
				sjn = calcCompletionTime(k);
				//sjn = getCompletionTime(k);
				selected = k;
			} else {
				if ((tmp = calcCompletionTime(k)) < sjn) {
				//if ((tmp = getCompletionTime(k)) < sjn) {
					sjn = tmp;
					selected = k;
				} else if (tmp == sjn) {
					// ties broken by FIFO
					if (pcbs[selected]->cTime > pcbs[k]->cTime) {
						sjn = tmp;
						selected = k;
					}
				}
			}
		}
	} // end for loop for process selection
	return selected;
}

// round robin scheduling for priority 2
void scheduleRR(int queue, char *arg1) {
	int i = 0;
	/*// quicker to do as global and count earlier, better practice less globals 
	for(i = 0; i < 18, i++) {
		if (pcbs[i] != NULL && pcbs[i]->priority == 2 && !pcbs[i]->isCompleted) {
			num++;
		}
	}*/
	if (num > 0) {
		fprintf(stderr, "Round-Robin Scheduling for %d processes.\n", num); 
	}
	for(i = 0; i < 18; i++) {
		if (pcbs[i] != NULL && pcbs[i]->priority == 2 && !pcbs[i]->isCompleted) {
			scheduleProcess(-1, arg1, i, num); // run process i for burst/num
		}
	}
}
// update clock for 1 iteration, or update by a custom millisec. amt
void updateClock(double r) {
	runInfo->lClock++;
	runInfo->lClock += r;

	fprintf(stderr, "oss: lClock: %.3f\n", runInfo->lClock); 
		
	sleep(1); // slow it down
}

// init and return pointer to shared pcb
pcb_t* initPcb() {
	int shm_id, r;
	pcb_t *pcb;
	srandom(time(NULL));

	if ((shm_id = shmget(IPC_PRIVATE, sizeof(pcb_t*), IPC_CREAT | 0755)) == -1) {
		perror("shmget");
		exit(1);
	}	
	if ((pcb = (pcb_t*) shmat(shm_id,0,0)) == (void*)-1) {
		perror("shmat");
		exit(1);
	}

	pcb->totalCpuTime = 0.000;
	pcb->totalSysTime = 0.000;
	pcb->cTime = runInfo->lClock;
	pcb->dTime = -1.000;
	pcb->lastBurstTime = -1.000;
	pcb->ioInterupt = false;
	pcb->priority = 0; // 0 high, 1 medium, 2 low
	pcb->isCompleted = false;
	pcb->shm_id = shm_id;
	r = rand() % 2; // between 0-1
	if (r < 1) {
		pcb->bound = io;
	} else { // r  > 1
		pcb->bound = cpu;
	}	
	if (pcb->bound == io) { // 1.000-3.999
		pcb->timeToComplete = (double)((rand() % 3) + 1); // 1-3 
		pcb->timeToComplete += (double)(rand() % 1000) / 1000;// 0-.999
	} else { // == cpu 4.000-6.999
		pcb->timeToComplete = (double) ((rand() % 3) + 4); // 4 - 6 
		pcb->timeToComplete += (double) (rand() % 1000) / 1000;// 0-.999
	}
	// semaphore to pause and resume the process
	if ((pcb->sem_id = semget(IPC_PRIVATE,1,IPC_CREAT | 0755)) == -1) {
		perror("semget");
	}
	if (initelement(pcb->sem_id,0,0) != 0) { // init to 0
		perror("semctl:initelement");
	}	
	return pcb;
} // end initPcb()

// detach and remove a pcb
void removePcb(pcb_t *pcbs[], int i) {
	int shm_id, n;
	if (pcbs[i] == NULL) {
		return;
	}
	// clean up semaphore
	if ((n = semctl(pcbs[i]->sem_id,0,IPC_RMID)) == -1) {
		perror("semctl:IPC_RMID:semaphore");
	}	
	// clean up shared memory
	shm_id = pcbs[i]->shm_id;
	if((n = shmdt(pcbs[i])) == -1) {
		perror("shmdt:pcb");
	}
	if((n = shmctl(shm_id, IPC_RMID, NULL)) == -1) {
		perror("shmctl:IPC_RMID:pcb");
	}
	pcbs[i] = NULL;
}

// call removePcb on entire array of pcbs
void cleanUpPcbs(pcb_t *pcbs[]) {
	int i;
	for(i = 0; i < 18; i++) {
		if (pcbs[i] != NULL) {
			removePcb(pcbs, i);
		}
	}
}

// clean up with free(), remove lClock, call cleanUpPcbs()
void cleanUp() {
	int shm_id = runInfo->shm_id;

	if ((shmdt(runInfo)) == -1) {
		perror("shmdt:runInfo");
	}
	if ((shmctl(shm_id, IPC_RMID, NULL)) == -1) {
		perror("shmctl:IPC_RMID:runInfo");	
	}

	cleanUpPcbs(pcbs);
	free(arg2);
	free(arg3);
	free(arg4);
	free(arg5);
}
// SIGINT handler
void free_mem() {
	int z;
	fprintf(stderr, "Recieved SIGINT. Cleaning up and quiting.\n");

	/*if (timed_out = true) {
		// to be safe add pid kill later
		system("killall userProcess");
	}*/
	// this instead?
	for (z = 0; z < 18; z++) {
		if (pcbs[z] != NULL) {
			if (pcbs[z]->pid != -1) {
				kill(pcbs[z]->pid,SIGINT);
				waitpid(pcbs[z]->pid,NULL,0);
			}
		}
	}
	// clean up with free(), remove lClock, call cleanUpPcbs()
	cleanUp();

	signal(SIGINT, SIG_DFL); // resore default action to SIGINT
	raise(SIGINT); // take normal action for SIGINT after my clean up
}

void timeout() {
	// timeout duration passed, send SIGINT
	//timed_out = true;
	fprintf(stderr, "Timeout duraction reached.\n");
	raise(SIGINT);
}
