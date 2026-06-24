#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    thread_data *ptr = (thread_data) thread_param;
    if ( ptr == NULL ) {
    	return NULL;
    }
    
    //sleep when started
    usleep(ptr->wait_to_obtain_ms);
    
    //obtain mutex
    if( pthread_mutex_lock(ptr->mutex) != 0 )
    {
    	ptr->thread_complete_success = false;
    	return ptr;
    }
    
    //wait in ms before releasing
    usleep(ptr->wait_to_release_ms);
    
    //release mutex
    if( pthread_mutex_unlock(ptr->mutex) != 0 )
    {
    	ptr->thread_complete_success = false;
    	return ptr;
    }
    
    ptr->thread_complete_success = true;
    return ptr;
    //finished exit the thread func
    //return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
     
    // Allocate memory for thread data structure
    thread_data *thread_data_ptr = (thread_data *)malloc(sizeof(thread_data));
    if (thread_data_ptr == NULL) {
        free(ptr);
        return false;
    }
    
    //create a thread and pass thread data as arguments
    int ret = pthread_create(thread, NULL, threadfunc, (void *)thread_data_ptr );
    if (ret !- 0 ){
      //free the memory when thread creation failed
    	free(thread_data_ptr);
    	return false;
    }
    
    return true;
}

