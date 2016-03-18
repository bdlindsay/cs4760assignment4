#include "oss.h"

// Brett Lindsay
// cs4860 assignment 4
// userProcess.c

pcb_t *pcb;
run_info_t *runInfo;
int process_num;

// prototypes 
void addToClock(double);

main (int argc, char *argv[]) {
	// argv[0] executable,[1] shared mem shm_id, [1] pcb shared mem
	bool willUseWholeQuantum; // true or false
	bool isProcessAbnormTerm; // only used after 50ms process time
	double amtOfQuantum;
	int shm_id; // to save shm_id
	int sem_id; // to save sem_id
	int r; // for random ints
	double rd; // for random doubles 
	process_num = atoi(argv[1]);
	srandom(time(NULL));

	//fprintf(stderr,"Got into Process %d\n", process_num);

	// get pcb info
	shm_id = atoi(argv[2]);
	if ((pcb = (pcb_t*) shmat(shm_id,0,0)) == (void*) -1) {
		perror("shmat:pcb");
	}
	if (pcb->isCompleted) {
		fprintf(stderr,"BAD ERROR OCCURED\n");
		exit(1);
	}

	// wait to be told to run
	sem_wait(pcb->sem_id,0);

	// get runInfo
	shm_id = atoi(argv[3]);
	if ((runInfo = (run_info_t*) shmat(shm_id,0,0)) == (void*) -1) {
		perror("shmat:runInfo");
	}
	
	fprintf(stderr,"%s : Proccess %d starting\n",argv[0],process_num);

	// willUseWholeQuantum
	r = (rand() % 10) + 1; // 1-10
	if (r > 3) { // most the time io proccesses won't use whole quantum
		willUseWholeQuantum = false;
	} else {
		willUseWholeQuantum = true;
	}	
	// opposite if cpu bound
	if (pcb->bound == cpu && willUseWholeQuantum) {
		willUseWholeQuantum = false;
	}	else if (pcb->bound == cpu && !willUseWholeQuantum) {
		willUseWholeQuantum = true;
	}
	// if not all, how much of quantum
	if (!willUseWholeQuantum) {
		r = (int) runInfo->burst * 1000; // change to int
		amtOfQuantum = (double)((rand() % r) + 1) / 1000; // back to double
		// io bound more likely not to finish, maintain priority
		if (pcb->bound == io) { 
			pcb->ioInterupt = true;
		}
		fprintf(stderr, "Determined partial quantum: %.3f\n", amtOfQuantum);
	}
	if (!willUseWholeQuantum) { // try portion whole quantum
		// if process is finished before time used
		if ((pcb->totalCpuTime + amtOfQuantum) > pcb->timeToComplete) {
			amtOfQuantum = pcb->timeToComplete - pcb->totalCpuTime;
			fprintf(stderr,"Will finish before partial quantum, new run time: %.3f\n",
				amtOfQuantum);
		}	
		addToClock(amtOfQuantum); // adding processing time case 1
	} else { // try whole quantum 
		// if process is finished before time used
		if ((pcb->totalCpuTime + runInfo->burst) > pcb->timeToComplete) {
			willUseWholeQuantum = false;
			amtOfQuantum = pcb->timeToComplete - pcb->totalCpuTime;
			fprintf(stderr,"Will finish before full quantum, new run time: %.3f\n", 
				amtOfQuantum);
			addToClock(amtOfQuantum);	// adding processing time case 2
		} else {	
			fprintf(stderr,"Processing for full quantum: %.3f\n",runInfo->burst);
			addToClock(runInfo->burst); // adding processing time case 3
		}
	}	

	// add the processing time
	if (willUseWholeQuantum) { // if it used the whole quantum
		pcb->totalCpuTime += runInfo->burst;
		pcb->lastBurstTime = runInfo->burst;
	} else { // if it used a partial quantum
		pcb->totalCpuTime += amtOfQuantum;
		pcb->lastBurstTime = amtOfQuantum;
	}
	//	update on where the process is at 
	fprintf(stderr,"Process %d progress: +%.3f -> %.3f/%.3f\n", 
		process_num, pcb->lastBurstTime, pcb->totalCpuTime, pcb->timeToComplete);

	// is this process complete, >= as safety
	if (pcb->totalCpuTime >= pcb->timeToComplete) {
		pcb->isCompleted = true;
		// set end stats for oss to collect
		pcb->dTime = runInfo->lClock;
		pcb->totalSysTime = pcb->dTime - pcb->cTime;
	} else if (pcb->totalCpuTime > .500){ // abnormal termination chance	
		r = rand() % 10; // 0 - 9
		if (r >= 7) { // 3/10 chance for abnormal termination
			pcb->isCompleted = true;
			pcb->dTime = runInfo->lClock;
			pcb->totalSysTime = pcb->dTime - pcb->cTime;
			fprintf(stderr, "Process %d had \"abnormal\" termination: %.3f/%.3f\n",
				process_num, pcb->totalCpuTime, pcb->timeToComplete);
		}		
	}
	if (pcb->isCompleted) {
		fprintf(stderr,"%s : Process %s quiting\n",argv[0], argv[1]);
	} else {
		fprintf(stderr,"%s : Process %s is pausing\n", argv[1], argv[1]);
	}
	
	// detach from runInfo
	shmdt(runInfo);
	runInfo = NULL;

	// detach from pcb
	shmdt(pcb);
	pcb = NULL;

	//exit(0);
} // end main()

void addToClock(double d) {
	//fprintf(stderr, "userProcess: Custom Clock adjustment : %.03f\n", d);
	runInfo->lClock += d;
	fprintf(stderr,"userProcess: lClock: %.03f\n", runInfo->lClock);
}	

// interupt handler
void intr_handler() {
	signal(SIGINT,SIG_DFL); // change SIGINT back to default handling

	/*if (msg != NULL) { // if allocated memory, free it
		free(msg);
	}
	if (fp != NULL) { // if file open, close it
		fclose(fp);
	}	*/
	if (pcb != NULL) {
		shmdt(pcb);
	}
	if (runInfo != NULL) {
		shmdt(runInfo);
	}
	fprintf(stderr,"Recieved SIGINT: Process %d cleaned up and dying.\n",
		process_num);

	// let it do default actions for SIGINT by resending now
	raise(SIGINT);
}
