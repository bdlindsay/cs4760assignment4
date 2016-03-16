#include "oss.h"
// Brett Lindsay
// cs4860 assignment 4
// userProcess.c

pcb_t *pcb;
int process_num;
lClock_t *lClock;
main (int argc, char *argv[]) {
	// argv[0] executable,[1] shared mem shm_id, [1] pcb shared mem
	bool willUseWholeQuantum;
	bool isProcessAbnormTerm; // only used after 50ms process time
	int timeQuantum; // from shm
	int amtOfQuantum;
	int shm_id;
	int sem_id;
	process_num = atoi(argv[1]);

	// get pcb info
	shm_id = atoi(argv[2]);
	if ((pcb = (pcb_t*) shmat(shm_id,0,0)) == (void*) -1) {
		perror("shmat");
	}

	sem_wait(pcb->sem_id, 0); // wait for signal to start from os 
	fprintf(stderr,"%s : Proccess %d starting\n",argv[0],process_num);
	sleep(2);
	fprintf(stderr,"%s : Process %s quiting\n",argv[0], argv[1]);
	pcb->isCompleted = true;
	

	// save sem_id and detach from pcb when finished
	sem_id = pcb->sem_id;
	shmdt(pcb);

	// signal oss to continue
	sem_signal(sem_id,0);

	//exit(0);
} // end

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
	fprintf(stderr,"Recieved SIGINT: Process %d cleaned up and dying.\n",
		process_num);

	// let it do default actions for SIGINT by resending now
	raise(SIGINT);
}
