#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tpool.h"

//the job 
struct job {
    int id;
    int dur;
};
/**
 * @brief The job in this case is just sleeping for the specified duration.
 * 
 * @param arg The job strucutre
 */
void job_fn (void *arg)
{
    struct job *job = arg;
    printf("Job ID = %d sleep duration = %d - START\n", job->id, job->dur);
    //sleep (job->dur);
    printf("Job ID = %d sleep duration = %d - END\n", job->id, job->dur);
}
/**
 * @brief Example of the destructor that can be provided
 * 
 * @param arg 
 */
void destructor (void *arg)
{
    free (arg);
}

int rand_sleep ()
{
    return (rand()%5) + 1;
}

int main()
{
    printf("Hello world!\n");

    //create a threadpool with 3 worker threads
    tpool_t *tpool = tpool_create (3);
    int i;
    int job_count = 7;
    struct job *job;

    for(i=0; i<job_count; i++) {
        //randomly generate and enqueue jobs
        job = malloc (sizeof(*job));
        job->id = i;
        job->dur = rand_sleep();

        /*
        here, two options are provided
            TPOOL_CLEANUP_RUN_JOB - on cleanup (during tpool destruction), the given job must be performed, 
                it cannot be discarded even if tpool_destroy is called
            TPOOL_RUN_DESTRUCTOR_AFTER_JOB - we want the provided destructor to perform cleanup always, since
                there is no cleanup code in job_fn
        */
        tpool_add_job (tpool, job_fn, job, destructor, TPOOL_CLEANUP_RUN_JOB | TPOOL_RUN_DESTRUCTOR_AFTER_JOB);
    }

    //sleep (10);

    printf("destroying threadpool\n");
    tpool_destroy (&tpool);
    printf("finished destroying threadpool\n");

    return 0;
}
