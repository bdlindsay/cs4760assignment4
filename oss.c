#include "oss.h"

// Brett Lindsay
// cs4760 assignment4
// oss.c

char *arg2; // to send process_id num to process
char *arg3; // to send shm_id num to process 
lClock_t *lClock;
pcb_t *pcbs[18] = { NULL };
run_info_t *runInfo;
bool timed_out = false;
// signal handler prototypes
void free_mem();
void timeout();

main (int argc, char *argv[]) {
	char *arg1 = "userProcess";
	arg2 = malloc(sizeof(int)); // process num in pcbs
	arg3 = malloc(sizeof(int)); // shm_id to pcb
	int r, i, shm_id;
	int usedPcbs[18] = { 0 }; // "bit vector" for used PIDs
	bool isPcbsFull = false;
	int next, nextCreate = 0; // points to next available PID
	srandom(time(NULL));
	signal(SIGINT, free_mem);
	signal(SIGALRM, timeout);
	alarm(60);	

	// create shared clock
	if((shm_id = shmget(IPC_PRIVATE,sizeof(lClock_t*),IPC_CREAT | 0755)) == -1) {
		perror("shmget:clock");
	}	
	lClock = (lClock_t*) shmat(shm_id,0,0);
	lClock->sec = 0;
	lClock->milli = 0;
	lClock->shm_id = shm_id;
	
	// create shared runInfo
	if((shm_id = shmget(IPC_PRIVATE,sizeof(run_info_t*),IPC_CREAT|0755)) == -1){
		perror("shmget:runinfo");
	}	
	runInfo = (run_info_t*) shmat(shm_id,0,0);
	runInfo->shm_id = shm_id;
	runInfo->lClock = (lClock_t*) shmat(shm_id,0,0);
	// set burst and process_num each time a process is chosen
	
	while(1) { // infinite loop until alarm finishes
		if (nextCreate < lClock->sec) {
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
				r = random() % 1000;	
				updateClock(r, false);
				continue;
			}
			// create and assign pcb to the OS's array
			// init PCB
			fprintf(stderr, "Creating new process at pcb[%d]\n",next);
			pcbs[next] = initPcb();
			usedPcbs[next] = true;
			sprintf(arg2, "%d", next); // process num in pcbs
			sprintf(arg3, "%d", pcbs[next]->shm_id); // shm_id for pcb
			if ((pcbs[next]->pid = fork()) == -1) {
				perror("fork");
				// clean up process if fork failed
				removePcb(pcbs, next);
				usedPcbs[next] = false;
				// update logical clock if fork fails
				r = random() % 1000;	
				updateClock(r, false);
				continue;
			}
			if (pcbs[next]->pid == 0) { // child process 
				execl("userProcess", arg1, arg2, arg3, 0);
			} // need else block to save pids?
			// only master process
			sem_signal(pcbs[next]->sem_id,0); // let userProcess start
			sem_wait(pcbs[next]->sem_id,0); // wait until its burst is done
			fprintf(stderr,"oss : OS is done waiting for userProcess!\n");

			// next process creation time
			r = rand() % 3; // 0-2
			nextCreate = lClock->sec + r;
		} // end create process if block
		
		// check to free a pcb
		for (i = 0; i < 18; i++) {
			if (pcbs[i] == NULL) {
				continue;
			}	
			fprintf(stderr, "oss: Checking if process %d is done: %d\n",i, 
				(bool)pcbs[i]->isCompleted);
			if (pcbs[i]->isCompleted) {
				// TODO collect data on userProcess
				fprintf(stderr,"oss: Removing pcb[%d]\n",i);
				removePcb(pcbs, i);
				usedPcbs[i] = false;
			}
		}
		// update logical clock
		r = random() % 1000;	
		updateClock(r, false);
	} // end infinite while	

	// cleanup after normal execution - never reached in current implementation
	cleanUp(); // clean up with free(), remove lClock, call cleanUpPcbs()
}

// which process should run next
void scheduleProcess(int queue) {
	static int i = 0;
	int next;
	for(i %= 18; i < 18; i++) {
		if(pcbs[i] == NULL) {
			fprintf(stderr,"scheduleProcess: %d is NULL\n");
			continue;
		}
		next = i;
		i++; // take turns
		break;
	}
	// set runInfo
	runInfo->process_num = next;
	runInfo->burst = 3; // seconds for now
	// to be safe if all processes are NULL
	if (i >= 18) {
		i %= 18;
	}	
// TODO determine shortest process for SJF strategey priority 0,1


// TODO setup round robin priority 2

}
// update clock for 1 iteration, or update by a custom millisec. amt
void updateClock(int r, bool isCustom) {
	if (!isCustom) {	
		lClock->sec++;
		lClock->milli += r;
		if(lClock->milli > 1000) {
			lClock->sec++;
			lClock->milli -= 1000;
		}
	} else {
		fprintf(stderr, "Custom Clock adjustment : %d\n", r);
		lClock->milli += r;
		while (lClock->milli > 1000) {
			lClock->sec++;
			lClock->milli -= 1000;
		}
	}
	fprintf(stderr,"Clock: %02d:%04d\n", lClock->sec, lClock->milli);
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

	r = rand() % 3; // between 0-2
	pcb->totalCpuTime = 0;
	pcb->totalSysTime = 0;
	pcb->cTime = lClock->sec;
	pcb->dTime = -1;
	pcb->lastBurstTime = -1;
	pcb->priority = 0; // 0 high, 1 medium, 2 low
	pcb->isCompleted = false;
	pcb->shm_id = shm_id;
	r = rand() % 2; // between 0-1
	if (r < 1) {
		pcb->bound = io;
	} else { // r  > 1
		pcb->bound = cpu;
	}	
	if (pcb->bound == io) {
		pcb->timeToComplete = (rand() % 750) + 250; // 250 - 1000
	} else { // == cpu
		pcb->timeToComplete = (rand() % 1000) + 1000; // 1000 - 2000
	}
	// semaphore to pause and resume the process
	if ((pcb->sem_id = semget(IPC_PRIVATE,1,IPC_CREAT | 0755)) == -1) {
		perror("semget");
	}
	if (initelement(pcb->sem_id,0,0) != 0) { // init to 0
		perror("semctl:initelement");
	}	
	return pcb;
}

// detach and remove a pcb
void removePcb(pcb_t *pcbs[], int i) {
	int shm_id, n;
	if (pcbs[i] == NULL) {
		return;
	}
	// clean up semaphore
	if ((n = semctl(pcbs[i]->sem_id,0,IPC_RMID)) != 0) {
		perror("semctl:IPC_RMID");
	}	
	// clean up shared memory
	shm_id = pcbs[i]->shm_id;
	if((shmdt((pcb_t*)pcbs[i])) == -1) {
		perror("shmdt");
	}
	pcbs[i] = NULL;
	if((shmctl(shm_id, IPC_RMID, NULL)) == -1) {
		perror("shmctl:IPC_RMID");
	}
}

// call removePcb on entire array of pcbs
void cleanUpPcbs(pcb_t *pcbs[]) {
	int i;
	for(i = 0; i < 18; i++) {
		removePcb(pcbs, i);
	}
}

// clean up with free(), remove lClock, call cleanUpPcbs()
void cleanUp() {
	int shm_id = runInfo->shm_id;
	if ((shmdt(runInfo->lClock)) == -1) {
		perror("shmdt:runInfo->lClock");
	}
	if ((shmdt(runInfo)) == -1) {
		perror("shmdt:runInfo");
	}
	if ((shmctl(shm_id, IPC_RMIC, NULL)) == -1) {
		
	}
	if ((shmdt(lClock)) == -1) {
		perror("shmdt:lClock");
	}
	if ((shmctl(shm_id, IPC_RMID,NULL)) == -1) {
		perror("shmctl:IPC_RMID");
	}
	cleanUpPcbs(pcbs);
	free(arg2);
	//free(arg3);

}
// SIGINT handler
void free_mem() {
	fprintf(stderr, "Recieved SIGINT. Cleaning up and quiting.\n");

	if (timed_out = true) {
		// to be safe add pid kill later
		system("killall userProcess");
	}
	// clean up with free(), remove lClock, call cleanUpPcbs()
	cleanUp();

	signal(SIGINT, SIG_DFL); // resore default action to SIGINT
	raise(SIGINT); // take normal action for SIGINT after my clean up
}

void timeout() {
	// timeout duration passed, send SIGINT
	timed_out = true;
	fprintf(stderr, "Timeout duraction reached.\n");
	raise(SIGINT);
}
