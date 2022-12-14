
/*
 * Multithreaded OS Simulation
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "student.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);



/**
 * current is an array of pointers to the currently running processes, 
 * each pointer corresponding to each CPU in the simulation.
 */
static pcb_t **current;
/* rq is a pointer to a struct you should use for your ready queue implementation.*/
static queue_t *rq;

static int ts;

/**
 * current and rq are accessed by multiple threads, so you will need to use 
 * a mutex to protect it (ready queue).
 *
 * The condition variable queue_not_empty has been provided for you
 * to use in conditional waits and signals.
 */
static pthread_mutex_t current_mutex;
static pthread_mutex_t queue_mutex;
static pthread_cond_t queue_not_empty;

/* keeps track of the scheduling alorightem and cpu count */
static sched_algorithm_t scheduler_algorithm;
static unsigned int cpu_count;

/**
 * enqueue() is a helper function to add a process to the ready queue.
 *
 * @param queue pointer to the ready queue
 * @param process process that we need to put in the ready queue
 */
void enqueue(queue_t *queue, pcb_t *process)
{
    /* FIX ME */
    if (scheduler_algorithm == PR) {
        if (is_empty(queue)) {
            pthread_mutex_lock(&queue_mutex);
            queue->head = process;
            queue->tail = process;
            pthread_cond_signal(&queue_not_empty);
            pthread_mutex_unlock(&queue_mutex);
        } else {
            if (queue->head->priority > process->priority) {
                pthread_mutex_lock(&queue_mutex);
                process->next = queue->head;
                queue->head = process;
                pthread_mutex_unlock(&queue_mutex);
            } else {
                pcb_t *curr = queue->head;
                while (curr->next != NULL && curr->next->priority < process->priority) {
                    curr = curr->next;
                }
                pthread_mutex_lock(&queue_mutex);
                process->next = curr->next;
                curr->next = process;   
                pthread_mutex_unlock(&queue_mutex);
            }
        }
    } else {
        if (is_empty(queue)) {
            pthread_mutex_lock(&queue_mutex);
            queue->head = process;
            queue->tail = process;
            pthread_cond_signal(&queue_not_empty);
            pthread_mutex_unlock(&queue_mutex);
        } else {
            pthread_mutex_lock(&queue_mutex);
            queue->tail->next = process;
            queue->tail = queue->tail->next;
            pthread_mutex_unlock(&queue_mutex);
        }
    }
}

/**
 * dequeue() is a helper function to remove a process to the ready queue.
 *
 * @param queue pointer to the ready queue
 */
pcb_t *dequeue(queue_t *queue)
{
    pcb_t *removed = queue->head;
    if (queue->head == queue->tail) {
        pthread_mutex_lock(&queue_mutex);
        queue->head = NULL;
        queue->tail = NULL;
        pthread_mutex_unlock(&queue_mutex);
    } else {
        pthread_mutex_lock(&queue_mutex);
        queue->head = queue->head->next;
        pthread_mutex_unlock(&queue_mutex); 
    }
    return removed;
}

/**
 * is_empty() is a helper function that returns whether the ready queue
 * has any processes in it.
 * 
 * @param queue pointer to the ready queue
 * 
 * @return a boolean value that indicates whether the queue is empty or not
 */
bool is_empty(queue_t *queue)
{
    /* FIX ME */
    if (queue->head == NULL && queue->tail == NULL) {
        return true;
    }
    return false;
}

/**
 * schedule() is your CPU scheduler.
 * 
 * Remember to specify the timeslice if the scheduling algorithm is Round-Robin
 * 
 * @param cpu_id the target cpu we decide to put our process in
 */
static void schedule(unsigned int cpu_id)
{
    if (is_empty(rq)) {
        pthread_mutex_lock(&current_mutex);
        current[cpu_id] = NULL;
        pthread_mutex_unlock(&current_mutex);
        context_switch(cpu_id, NULL, -1);
    } else if (scheduler_algorithm == FCFS) {
        pcb_t *deq = dequeue(rq);
        pthread_mutex_lock(&current_mutex);
        current[cpu_id] = deq;
        current[cpu_id]->state = PROCESS_RUNNING;
        pthread_mutex_unlock(&current_mutex);
        context_switch(cpu_id, deq, -1);
    } else if (scheduler_algorithm == RR) {
        pcb_t *deq = dequeue(rq);
        pthread_mutex_lock(&current_mutex);
        current[cpu_id] = deq;
        current[cpu_id]->state = PROCESS_RUNNING;
        pthread_mutex_unlock(&current_mutex);
        context_switch(cpu_id, deq, ts);
    } else if (scheduler_algorithm == PR) {
        pcb_t *deq = dequeue(rq);
        pthread_mutex_lock(&current_mutex);
        current[cpu_id] = deq;
        current[cpu_id]->state = PROCESS_RUNNING;
        pthread_mutex_unlock(&current_mutex);
        context_switch(cpu_id, deq, -1);
    }

}

/**
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * @param cpu_id the cpu that is waiting for process to come in
 */
extern void idle(unsigned int cpu_id)
{
    /* FIX ME */
    pthread_mutex_lock(&queue_mutex);
    while (is_empty(rq)) {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }
    pthread_mutex_unlock(&queue_mutex);
    schedule(cpu_id);
    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
}

/**
 * preempt() is the handler used in Round-robin and Preemptive Priority 
 * Scheduling
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 * 
 * @param cpu_id the cpu in which we want to preempt process
 */
extern void preempt(unsigned int cpu_id)
{
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_READY;
    enqueue(rq, current[cpu_id]);
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}

/**
 * yield() is the handler called by the simulator when a process yields the
 *  CPU to perform an I/O request.
 *
 * @param cpu_id the cpu that is yielded by the process
 */
extern void yield(unsigned int cpu_id)
{
    /* FIX ME */
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}

/**
 * terminate() is the handler called by the simulator when a process completes.
 * 
 * @param cpu_id the cpu we want to terminate
 */
extern void terminate(unsigned int cpu_id)
{
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}

/**
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes. This method will also need to handle priority, 
 * Look in section 5 of the PDF for more info.
 * 
 * @param process the process that finishes I/O and is ready to run on CPU
 */
extern void wake_up(pcb_t *process)
{

    process->state = PROCESS_READY;
    enqueue(rq, process);
    

}


/**
 * main() simply parses command line arguments, then calls start_simulator().
 * Add support for -r and -p parameters. If no argument has been supplied, 
 * you should default to FCFS.
 * 
 * HINT:
 * Use the scheduler_algorithm variable (see student.h) in your scheduler to 
 * keep track of the scheduling algorithm you're using.
 */
int main(int argc, char *argv[])
{

    /*
     * FIX ME
     */
    scheduler_algorithm = FCFS;
    if (argc > 4 || argc == 1) 
    {
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
                        "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
                        "    Default : FCFS Scheduler\n"
                        "         -r : Round-Robin Scheduler\n1\n"
                        "         -p : Priority Scheduler\n");
        return -1;
    }   else if (argc == 2) {
        scheduler_algorithm = FCFS;
    } else if (argc == 4 && (argv[2][1]) == 'r') {
        scheduler_algorithm = RR;
        ts = atoi(argv[3]);
    } else if (argc == 4 && (argv[2][1] == 'p')) {
        scheduler_algorithm = PR;
    }

    cpu_count = strtoul(argv[1], NULL, 0);


    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t *) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queue_not_empty, NULL);
    rq = (queue_t *)malloc(sizeof(queue_t));
    assert(rq != NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}

#pragma GCC diagnostic pop