/* MIT License

Copyright (c) [2020] [Ashwin Natarajan]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include "tpool.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
/************************************************************************************/
//remove asserts in non debug build
#if !defined(TPOOL_DEBUG) && !defined(NDEBUG)
#define NDEBUG
#endif // TPOOL_DEBUG

#define TPOOL_SUCCESS       0
#define TPOOL_FAILURE       -1

#define TPOOL_TRUE          1
#define TPOOL_FALSE         0
/************************************************************************************/
//private structures
/**
 * @brief           the struct that holds the info regarding a job
 * @var fn_ptr      The job function pointer
 * @var destructor  The optional function pointer for the destructor
 * @var arg         The optional pointer that holds the pointer to the arg
 * @var opt         The bitwise options data
 */
struct _tpool_job_s {
    void (*fn_ptr) (void*);
    void (*destructor) (void*);
    void *arg;
    int opt;

    struct _tpool_job_s     *prev;
    struct _tpool_job_s     *next;
};
/**
 * @brief           The struct that holds the queue of the jobs. Jobs enter the queue from the back
 *                      and exit the queue from the front
 * @var front       The front of the queue - the jobs exit the queue through this side
 * @var back        The back of the queue - the jobs enter the queue through this side
 * @var lock        The pthread mutex lock for thread safe access of the queue
 * 
 */
struct _tpool_q_s {
    struct _tpool_job_s     *front;
    struct _tpool_job_s     *back;
    pthread_mutex_t         lock;
};
//the actual threadpool struct
/**
 * @brief           The struct that is typedef'd to tpool_t
 * @var tpool_sem   The sempahore for synchronising the worker threads
 * @var queue       The instance of the queue structure
 * @var tcount      Holds the number of threads in this tpool
 * @var threads     Pointer to the array of pthread_t for each worker thread
 * @var status      TPOOL_TRUE => tpool initialised, else not initialised
 * @var exit_flag   Holds TPOOL_TRUE if tpool_destroy is called
 * 
 */
struct _tpool_s {
    sem_t                   tpool_sem;
    struct _tpool_q_s       queue;
    int                     tcount;
    pthread_t               *threads;
    int                     status;
    int                     exit_flag;
};
/************************************************************************************/
//static helper function declarations
static void *_tpool_thread (void *arg);
static void _tpool_enqueue (struct _tpool_q_s *queue, struct _tpool_job_s *job);
static struct _tpool_job_s* _tpool_dequeue (struct _tpool_q_s *queue);
/************************************************************************************/
//public function definitions
/**
 * @brief Creates and initialises a threadpool with the specified number of worker threads
 * 
 * @param count Specifies the number of worker threads required for this tpool
 * @return tpool_t* Pointer to the tpool - this will be the unique handle for this tpool
 */
tpool_t* tpool_create (int count)
{
    tpool_t *ret = malloc (sizeof (*ret));
    int i;
    //if allocation of the tpool memory was successful
    if(ret) {
        ret->status = TPOOL_FAILURE;
        do {
            //if sem_init fails, return NULL, leak no memory
            if(sem_init (&(ret->tpool_sem), 0, 0) == TPOOL_FAILURE) {
                perror("sem_init");
                free(ret);
                ret = NULL;
                break;
            }

            //if mutex_init fails, return NULL, leak no memory
            if(pthread_mutex_init(&(ret->queue.lock), NULL) == TPOOL_FAILURE) {
                perror("pthread_mutex_init");
                sem_destroy (&(ret->tpool_sem));
                free(ret);
                ret = NULL;
                break;
            }
            //init queue pointers
            ret->queue.front = NULL;
            ret->queue.back  = NULL;

            //allocate threads array
            ret->threads = malloc (count * sizeof(*(ret->threads)) );
            if(ret->threads == NULL) {
                perror("malloc");
                pthread_mutex_destroy (&(ret->queue.lock));
                sem_destroy (&(ret->tpool_sem));
                free(ret);
                ret = NULL;
                break;
            }
            ret->tcount = count;

            //create threads
            for(i=0; i<count; i++) {
                pthread_create (&(ret->threads[i]), NULL, _tpool_thread, ret);
            }

            ret->status = TPOOL_SUCCESS;
            ret->exit_flag  = TPOOL_FALSE;
        }while(0);  //do while(0) trick to avoid goto statement
    }
    else {
        perror("malloc");
    }

    return ret;
}
/* <==========================================> */
/**
 * @brief               Adds the given job to the thread pool. Will fail if 
 *                          tpool is not properly initialised
 *                          job_fn is NULL
 *                          memory allocation fails * 
 * @param tpool         The handle to the tpool
 * @param job_fn        The function pointer for the job to be performed
 * @param arg           The (optional) arg for job_fn
 * @param destructor    The (optional) destructor for job_fn
 * @param opt           The options for job to be performed
 * @return int          Returns 0 on success, -1 on failure
 */
int tpool_add_job (tpool_t *tpool, void (*job_fn)(void *), void *arg, void (*destructor)(void*), int opt)
{
    int ret = TPOOL_FAILURE;
    if(tpool && job_fn && tpool->status == TPOOL_SUCCESS) {
        struct _tpool_job_s *job = malloc (sizeof(*job));
        if(job) {
            //prepare the job struct
            job->fn_ptr     = job_fn;
            job->arg        = arg;
            job->destructor = destructor;
            job->opt        = opt;

            job->next       = NULL;
            job->prev       = NULL;

            //add job and notify the workers
            _tpool_enqueue(&tpool->queue, job);
            sem_post (&(tpool->tpool_sem));
            ret = TPOOL_SUCCESS;
        }
        else {
            perror("malloc");
        }
    }
    return ret;
}
/* <==========================================> */
/**
 * @brief           Destroys the given thread pool and its associated sync variables. Can fail if
 *                      ->tpool is NULL or *tpool is NULL
 *                      ->any cleanup fails
 *                  The destructor for a job will be run regardless of options since the thread pool is being 
 *                      forcibly destroyed
 * 
 * @param tpool     The thread pool to be destroyed. takes tpool_t** because the tpool_t* will also be freed at the end
 * @return int      Returns 0 on Success, -1 on failure
 */
int tpool_destroy(tpool_t **tpool)
{
    if(tpool == NULL || *tpool == NULL) {
        return TPOOL_FAILURE;
    }
    //set exit flag and notify threads
    (*tpool)->exit_flag = TPOOL_TRUE;
    int i;
    //post the sem tcount times so that all threads wake up
    for(i=0; i<(*tpool)->tcount; i++) {
        sem_post (&((*tpool)->tpool_sem));
    }

    //join all threads
    int ret = TPOOL_SUCCESS;
    for(i=0; i<(*tpool)->tcount; i++) {
        pthread_join (((*tpool)->threads[i]), NULL);
    }
    //free the threads list
    free((*tpool)->threads);
    (*tpool)->tcount = 0;(*tpool)->exit_flag = TPOOL_FALSE; 

    //destroy semaphore
    if(sem_destroy (&((*tpool)->tpool_sem)) == TPOOL_FAILURE) {
        perror("sem_destroy");
        ret = TPOOL_FAILURE;
    }

    //empty the queue
    struct _tpool_job_s *job;
    do {
        job = _tpool_dequeue(&(*tpool)->queue);
        if(job) {
            //perform the job if requested for
            if(job->opt & TPOOL_CLEANUP_RUN_JOB) {
                (job->fn_ptr) (job->arg);
            }
            //cleanup if requested for
            if(job->arg && job->destructor && (job->opt & TPOOL_RUN_DESTRUCTOR_AFTER_JOB) ) {
                (job->destructor) (job->arg);
            }
            free(job);
        }
    }while(job);

    //destroy queue mutex
    if(pthread_mutex_destroy (&((*tpool)->queue.lock)) == TPOOL_FAILURE) {
        perror("pthread_mutex_destroy");
        ret = TPOOL_FAILURE;
    }

    free(*tpool);
    *tpool = NULL;

    return ret;
}
/************************************************************************************/
/** @brief The thread function for the thread pool worker
 * 
 * @param *arg  Pointer to the thread pool struct, since some of its sync variables are required
 * @return void* always returns NULL
 */
static void *_tpool_thread (void *arg)
{
    tpool_t *tpool = arg;
    int status;
    struct _tpool_job_s *job;
    while(1) {
        //wait for job
        status = sem_wait (&(tpool->tpool_sem));
        if(status == TPOOL_FAILURE) {
            perror("sem_wait");
            break;
        }

        //if exit_flag is set, break out of the loop
        if(tpool->exit_flag == TPOOL_TRUE) {
            break;
        }

        //get and process job
        job = _tpool_dequeue(&(tpool->queue));
        if(job) {
            (job->fn_ptr(job->arg));
            //if destructor calling is requested for, do it
            if( (job->opt & TPOOL_RUN_DESTRUCTOR_AFTER_JOB) && job->destructor && job->arg ) {
                job->destructor(job->arg);
            }
            //free the job
            free(job);
        }
    }
    return NULL;
}
/* <==========================================> */
/**
 * @brief       Adds a job to the given queue, thread safe
 * 
 * @param queue queue to which the job needs to be added
 * @param job   the prepared job structure
 */
static void _tpool_enqueue (struct _tpool_q_s *queue, struct _tpool_job_s *job)
{
    //it is assumed that queue and job are not NULL, since checks are performed in the caller
    //it is assumed that the caller has set the job struct links to NULL.
    pthread_mutex_lock (&(queue->lock));

    //add jobs through the back
    if(queue->front == NULL) {
        //it is a bug if front is pointing to NULL and back is pointing to something
        assert (queue->back == NULL);

        queue->front = job;
        queue->back  = job;
    }
    else {
        //it is a bug if front is pointing to something and back is pointing to NULL
        assert (queue->back != NULL);

        //add job to the back of the queue, update the links
        queue->back->next   = job;
        job->prev           = queue->back;
        queue->back         = job;
    }
    //don't forget to unlock the mutex
    pthread_mutex_unlock (&(queue->lock));
}
/* <==========================================> */
/**
 * @brief       Remove a job from the queue, thread safe
 * 
 * @param tpool the tpool
 * @return      struct _tpool_job_s* pointer to the job, returns NULL if queue is empty
 */
static struct _tpool_job_s* _tpool_dequeue (struct _tpool_q_s *queue)
{
    //assumes that queue is not NULL and initialised fully, since checks are performed in the caller
    struct _tpool_job_s *ret = NULL;

    //mutex lock the queue
    pthread_mutex_lock (&(queue->lock));

    //check if list is empty
    if(queue->front) {
        //if front is pointing to something, the back should not point to NULL
        assert (queue->back != NULL);

        //remove node from the front
        ret = queue->front;

        //update the links
        //if there is only 1 job in the queue, set the queue pointers to NULL
        if(queue->front == queue->back) {
           queue->front  = NULL;
           queue->back   = NULL;
        }
        //else move the front pointer one step back
        else {
            queue->front        = queue->front->next;
            //update links
            queue->front->prev  = NULL;
        }

        //clean up return variable so that links aren't exposed to the caller
        ret->prev                   = NULL;
        ret->next                   = NULL;
    }
    else {
        //if front is pointing to NULL, back must also be pointing to NULL
        assert (queue->back == NULL);
    }

    //unlock queue when done
    pthread_mutex_unlock (&(queue->lock));
    return ret;
}
