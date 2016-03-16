#include "semaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>

// Brett Lindsay
// cs4760 assignment 4
// oss.h

typedef enum {false, true} bool;
typedef enum {io,cpu} pType;

// a process control block
typedef struct {
	long totalCpuTime; // time processing
	long totalSysTime; // time in system
	int cTime; // create time
	int dTime; // destroy time
	long lastBurstTime; 
	int priority; // 0 high, 1 medium, 2 low
	bool isCompleted;
	int shm_id; // ref to id for shm
	int pid; // pid for fork() return value
	pType bound; // io or cpu 
	long timeToComplete; // varies based on pType
	int sem_id; // hold semaphore id
	//semaphore sem; part of semaphore set referenced to sem_id
} pcb_t;	

// logical clock
typedef struct {
	unsigned int sec;
	unsigned int milli; 
	int shm_id;
} lClock_t;

typedef struct {
	int process_num;
	long burst; // burst time in ms
	lClock_t *lClock; // lClock shm_id
	int shm_id; // run_info_t shm_id 
} run_info_t;

// helper functions
pcb_t* initPcb();
void updateClock(int,bool);
void cleanUpPcbs(pcb_t *pcbs[]);
void cleanUp();
void removePcb(pcb_t *pcbs[], int i);
