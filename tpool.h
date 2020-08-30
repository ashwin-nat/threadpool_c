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

#ifndef __TPOOL_H__
#define __TPOOL_H__
/************************************************************************************/
/**
 * @brief options that can be provided to tpool_add_job and tpool_destroy
 * 
 */
#define TPOOL_NO_OPT                        0
#define TPOOL_CLEANUP_RUN_JOB               (1<<0)
/**
 * A destructor can be provided to cleanup arguments on the event of tpool_destroy().
 * The below option is used to inform the code that this same destructor must be used if the job
 * has been performed normally. Perhaps, a more elegant solution can be thought of for this. 
 */
#define TPOOL_RUN_DESTRUCTOR_AFTER_JOB      (1<<1)
/************************************************************************************/
/**
 * @brief The opaque structure that will be the handle to a thread pool. The caller does not need to
 *              know the struct contents
 * 
 */
typedef struct _tpool_s tpool_t;
/************************************************************************************/
//function declarations
/**
 * @brief Creates and initialises a threadpool with the specified number of worker threads
 * 
 * @param count Specifies the number of worker threads required for this tpool
 * @return tpool_t* Pointer to the tpool - this will be the unique handle for this tpool
 */
tpool_t* tpool_create (int count);

/**
 * @brief               Adds the given job to the thread pool. Will fail if 
 *                          tpool is not properly initialised
 *                          job_fn is NULL
 *                          memory allocation fails * 
 * @param tpool         The handle to the tpool
 * @param job_fn        The function pointer for the job to be performed
 * @param arg           The (optional) arg for job_fn
 * @param destructor    The (optional) destructor for arg
 * @param opt           The options for job to be performed
 * @return int          Returns 0 on success, -1 on failure
 */
int tpool_add_job (tpool_t *tpool, void (*job_fn)(void *), void *arg, void (*destructor)(void*), int opt);

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
int tpool_destroy (tpool_t **tpool);
/************************************************************************************/
#endif // __TPOOL_H__
