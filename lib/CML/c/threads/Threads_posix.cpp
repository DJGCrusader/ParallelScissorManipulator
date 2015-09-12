/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "CML.h"
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

// Max time to wait in ms before checking for thread exit
#define MAX_WAIT   100

CML_NAMESPACE_USE();

// Thread specific local data
struct PosixThreadData
{
   Thread *thread;

   // Posix thread structure
   pthread_t pthread;

   // True if the thread is being requested to exit
   bool pleaseStop;

   // Semaphore used to wait for the thread to finish
   Semaphore stopSem;

   PosixThreadData( Thread *t ): stopSem(0)
   {
      thread = t;
      pleaseStop = false;
   }
};

// Manages thread local storage.  A class is used for this so 
// that the key can be properly initialized at system startup 
// using a static constructor
class TlsKey
{
   pthread_key_t key;
public:
   TlsKey( void )
   {
      pthread_key_create( &key, 0 );
   }
   ~TlsKey()
   {
      pthread_key_delete( key );
   }
   void Set( void *ptr )
   {
      pthread_setspecific( key, ptr );
   }
   void *Get( void )
   {
      return pthread_getspecific( key );
   }
};

/**************************************************
* This class is used to terminate the running thread.
* It's thrown as an exception, and caught in the 
* thread starter stub.  This allows my stack to 
* unwind properly in a thread that is stopped.
**************************************************/
class ThreadExitException
{
};

/* local functions */
static void *ThreadStarter( void *arg );

/* local data */
static TlsKey tls;

/***************************************************************************/
/**
Default constructor for a new thread object.  For the pthreads version of
this class the default constructor doesn't do anything.
*/
/***************************************************************************/
Thread::Thread( void )
{
   priority = 5;
   data = 0;
}

/***************************************************************************/
/**
Destructor for a thread.  Causes the thread to be cancelled and waits for
it to finish.
*/
/***************************************************************************/
Thread::~Thread( void )
{
   stop(100);
}

/***************************************************************************/
/**
Set the threads priority.  The priority must be specified in the range 0 
(lowest priority) to 9 (highest priority).  The default thread priority 
is 5 and this will be used if the priority is not explicitely set.

This funciton must be called before the thread is started.  Calling it
after the thread has already started will have no effect.

@param pri The thread's priority, in the range 0 to 9.
@return A valid error object.
*/
/***************************************************************************/
const Error *Thread::setPriority( int pri )
{
   if( pri < 0 || pri > 9 )
      return &ThreadError::BadParam;

   priority = pri;

   return 0;
}

/***************************************************************************/
/**
Start this thread.  The thread's virtual run() function will be called when
the thread has started.
*/
/***************************************************************************/
const Error *Thread::start( void )
{
   if( data ) return &ThreadError::Running;

   PosixThreadData *tData = new PosixThreadData( this );
   if( !tData ) return &ThreadError::Alloc;
   data = tData;

   int min, max, inc;
   min = sched_get_priority_min( SCHED_FIFO );
   max = sched_get_priority_max( SCHED_FIFO );
   inc = (max-min+5)/10;

   struct sched_param sched;
   sched.sched_priority = min + priority * inc;

   if( sched.sched_priority < min ) sched.sched_priority = min;
   if( sched.sched_priority > max ) sched.sched_priority = max;

   pthread_attr_t attr;
   pthread_attr_init( &attr );
   pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
   pthread_attr_setschedpolicy( &attr, SCHED_FIFO );
   pthread_attr_setschedparam( &attr, &sched );

   int ret = pthread_create( &tData->pthread, &attr, ThreadStarter, tData );

   // If this fails, I'll try a second time with default attributes.  
   if( ret )
   {
      cml.Error( "pthread_create error %d creating a thread of pri %d.  Retrying...\n", ret, priority );
      ret = pthread_create( &tData->pthread, 0, ThreadStarter, tData );
   }

   // If this still fails, I'll return an error.
   if( ret )
   {
      cml.Error( "pthread_create error %d creating a thread with default attributes.\n", ret );
      return &ThreadError::Start;
   }
  
   //detatch pthread, so memory can be reclaimed upon termination
   pthread_detach(tData->pthread);

   return 0;
}

/***************************************************************************/
/**
Stop this thread.
*/
/***************************************************************************/
const Error *Thread::stop( Timeout to )
{
   // If the data pointer is null, the thread must have already been stopped
   if( !data ) return 0;

   // If the stop method is called from the context of the thread
   // being stopped, just throw an exception.  This unwraps the 
   // thread's stack.
   if( data == tls.Get() )
      throw ThreadExitException();

   // Set a flag in the thread's data structure asking for it to stop.
   PosixThreadData *tData = (PosixThreadData *)data;
   tData->pleaseStop = true;
   data = 0;

   // Wait for the thread to post a semaphore indicating that it's stopped.
   const Error *err = tData->stopSem.Get( to );
   if( !err ) delete tData;

   return err;
}

/***************************************************************************/
/**
Put the thread to sleep for a specified amount of time.
@param timeout The time to sleep in milliseconds.  If <0 the thread will
       sleep forever.
*/
/***************************************************************************/
const Error *Thread::sleep( Timeout to )
{
   if( to == 0 ) return 0;

   // Create a semaphore that no-one will ever post to
   // and wait on that with a timeout
   Semaphore sem(0);

   const Error *err = sem.Get( to );

   // Convert timeout errors to no error since
   // that's what we expect to see.
   if( err == &ThreadError::Timeout )
      err = 0;

   return err;
}

/***************************************************************************/
/**
Return the current time in millisecond units
*/
/***************************************************************************/
uint32 Thread::getTimeMS( void )
{
   struct timeval tv;
   gettimeofday( &tv, 0 );

   return (uint32)( tv.tv_sec*1000 + tv.tv_usec/1000 );
}


void Thread::__run( void )
{
   run();

   // If the run method returns, delete the threads local data.
   // Note that this won't happen here if the thread is stopped.
   delete (PosixThreadData *)data;
   data = 0;
}

/***************************************************************************/
/**
Create a new mutex object.
*/
/***************************************************************************/
Mutex::Mutex( void )
{
   data = new pthread_mutex_t;

   if( data )
   {
      pthread_mutexattr_t attr;
      pthread_mutexattr_init(&attr);
      pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
      pthread_mutex_init( (pthread_mutex_t *)data, &attr );
   }
   else
      cml.Error( "Unable to allocate memory for mutex\n" );
}

/***************************************************************************/
/**
Destructor for mutex object.  
*/
/***************************************************************************/
Mutex::~Mutex( void )
{
   if( data )
   {
      pthread_mutex_destroy( (pthread_mutex_t *)data );
      delete (pthread_mutex_t *)data;
   }
}

/***************************************************************************/
/**
Lock this mutex
*/
/***************************************************************************/
const Error *Mutex::Lock( void )
{
   CML_ASSERT( data != 0 );
   if( !data ) return &ThreadError::Alloc;

   int err = pthread_mutex_lock( (pthread_mutex_t *)data );

   if( err )
      cml.Warn( "pthread_mutex_lock returned %d\n", err );

   return 0;
}

/***************************************************************************/
/**
Unlock the mutex
*/
/***************************************************************************/
const Error *Mutex::Unlock( void )
{
   CML_ASSERT( data != 0 );
   if( !data ) return &ThreadError::Alloc;

   int err = pthread_mutex_unlock( (pthread_mutex_t *)data );
   if( err )
      cml.Warn( "pthread_mutex_unlock returned %d\n", err );
   return 0;
}

/***************************************************************************/
/**
Default constructore for a semaphore object.  Initializes the semaphore to
it's default attributes.
@param count The initial count of the semaphore.  The semaphore's Get method
may be called that many times before any thread will block on the semaphore.
*/
/***************************************************************************/
Semaphore::Semaphore( int32 count )
{
   data = new sem_t;

   if( data )
      sem_init( (sem_t *)data, 0, count );
   else
      cml.Error( "Unable to allocate memory for semaphore\n" );
}

/***************************************************************************/
/**
Destructor for Semaphore object.
*/
/***************************************************************************/
Semaphore::~Semaphore( void )
{
   if( data )
   {
      sem_destroy( (sem_t *)data );
      delete (sem_t *)data;
   }
}

// Wait on a semaphore with a timeout.  This is repeatedly called
// with a short timeout when waiting on a semaphore.
static const Error *SemaWait( sem_t *sem, Timeout to )
{
   struct timespec ts;

   int err = clock_gettime( CLOCK_REALTIME, &ts );
   if( err ) return &ThreadError::General;

   to += (ts.tv_nsec / 1000000);
   long sec = to / 1000;
   to -= sec * 1000;

   ts.tv_sec += sec;
   ts.tv_nsec = to * 1000000;

   err = sem_timedwait( sem, &ts );
   if( !err ) return 0;

   if( errno == ETIMEDOUT )
      return &ThreadError::Timeout;
   return &ThreadError::General;
}

/***************************************************************************/
/**
Get the semaphore with an optional timeout.  An error is returned if the 
timeout expires before the semaphore is acquired.
@param timeout The timeout in milliseconds.  Any negative value will cause
       the thread to wait indefinitely.
@return An error code indicating success or failure.
*/
/***************************************************************************/
const Error *Semaphore::Get( Timeout to )
{
   // Make sure the semaphore was correctly initialized
   CML_ASSERT( data != 0 );
   if( !data ) return &ThreadError::Alloc;

   // Set a positive timeout if waiting forever
   bool forever = (to < 0);
   if( forever ) to = 2*MAX_WAIT;

   PosixThreadData *tData = (PosixThreadData *)tls.Get();

   do
   {
      // Before waiting, make sure the thread isn't being requested to exit.
      // If so I'll throw an exception here which is caught by the top level 
      // function that started the threads.  Things should be cleaned up as the
      // stack unwinds on the way up.
      if( tData && tData->pleaseStop )
         throw ThreadExitException();

      // Wait on the semaphore with a short timeout
      Timeout T = (to<MAX_WAIT) ? to : MAX_WAIT;
      const Error *err = SemaWait( (sem_t *)data, T );
      if( err != &ThreadError::Timeout )
         return err;

      // Reduce remaining timeout if not waiting forever
      if( !forever ) to -= T;

   } while( to > 0 );

   return &ThreadError::Timeout;
}

/***************************************************************************/
/**
Increase the count of the semaphore object.  If any threads are pending on 
the object, then the highest priority one will be made eligable to run.
@return An error object
*/
/***************************************************************************/
const Error *Semaphore::Put( void )
{
   CML_ASSERT( data != 0 );
   if( !data ) return &ThreadError::Alloc;

   if( sem_post( (sem_t*)data ) )
      return &ThreadError::General;

   return 0;
}

/// Static function used to start a new thread.
static void *ThreadStarter( void *arg )
{
   PosixThreadData *tData = (PosixThreadData *)arg;

   tls.Set( tData );

   try
   {
      tData->thread->__run();
   }
   catch( ThreadExitException )
   {
      tData->stopSem.Put();
   }

   tls.Set( NULL );
   return NULL;
}

