/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/**
\file
This file holds the code needed to implement the CML reference counting objects.
*/

#include "CML.h"

#define ADD_NAMES_TO_REFINFO

CML_NAMESPACE_START()
struct RefInfo
{
   int refCt;          // Number of open references
   int lockCt;         // Number of locks for this reference
   bool autoDeleteEna; // If true, delete object when refCt == 1

#ifdef ADD_NAMES_TO_REFINFO
   const char *name;   // Reference name for debugging
#endif

   union
   {
      RefObj *ptr;     // Used when this object is active
      int32 next;      // Used when on the recycled list
   };
};

CML_NAMESPACE_END()

CML_NAMESPACE_USE();

// We keep track of our reference information using some dynamically 
// allocated structures maintained locally.  The following defines 
// control the maximum number of these structures that we will allocate
// before failing.
//
// The structures themselves are allocated in blocks.  This setting controls
// how many reference structures are allocated at a time.
#define CML_REF_BITS                10   // 1024 ref/block

// This setting defines the maximum number of blocks of references I'll allocate
// before failing.
#define MAX_BLOCKS                  1024

#define REF_PER_BLOCK               (1<<CML_REF_BITS)

static RefInfo *refBlocks[ MAX_BLOCKS ];
static uint16 totAllocBlocks = 0;
static uint16 remainInBlock = 0;
static int32 recycleList = -1;
static int32 nextNewRef = 0;

// local functions
static RefInfo *GetRefInfoPtr( int32 id );
static void ReleaseRefInfo( RefInfo *ri, int32 id );

// This function replaces the previous static declaration of the reference
// table mutex.  This was changed to avoid static object initialization
// order dependencies.
// The mutex is allocated on the heap and never released.  We expect the OS
// to clean this up when the program exits.
static Mutex &refTblMtx( void )
{
   static Mutex *tblMtx = new Mutex();
   return *tblMtx;
}

/***************************************************************************/
/**
Default constructor for a reference object.
This allocates a reference ID which will be associated with this object for
as long as any references to the object exist.  This ID can be safely used 
to find a pointer to the object.

@param name  An optional string that identifies the reference for debugging 
             purposes.  If a string is passed it should persist for the entire
             life of the reference object.

*/
/***************************************************************************/
RefObj::RefObj( const char *name )
{
   refID = -1;
   this->name = name;

   MutexLocker ml( refTblMtx() );

   // Find a structure to hold local info about this 
   // reference.  Check the recycle list first.
   RefInfo *ri = GetRefInfoPtr( recycleList );
   if( ri )
   {
      refID = recycleList;
      recycleList = ri->next;
   }

   // See if I need to allocate a new block
   else if( !remainInBlock )
   {
      CML_ASSERT( totAllocBlocks < MAX_BLOCKS );
      if( totAllocBlocks >= MAX_BLOCKS )
      {
         cml.Error( "Unable to allocate any more reference blocks!\n" );
         return;
      }

      refBlocks[ totAllocBlocks ] = new RefInfo[ REF_PER_BLOCK ];
      CML_ASSERT( refBlocks[totAllocBlocks] );

      if( !refBlocks[totAllocBlocks] )
      {
         cml.Error( "Failed to allocate new reference block!\n" );
         return;
      }

      totAllocBlocks++;
      remainInBlock = REF_PER_BLOCK;

      cml.Debug( "Just allocated reference block %d\n", totAllocBlocks );
   }

   // If I didn't get a reference off the recycle list, grab the
   // next available one.
   if( refID < 0 )
   {
      refID = nextNewRef++;
      remainInBlock--;
      ri = GetRefInfoPtr( refID );
   }

   CML_ASSERT( ri );

   // Initialize the reference info structure
   ri->refCt         = 1;
   ri->lockCt        = 0;
   ri->ptr           = this;
   ri->autoDeleteEna = false;

#ifdef ADD_NAMES_TO_REFINFO
   ri->name  = name;
#endif
}

/***************************************************************************/
/**
Reference object destructor.  If this object has been locked then this 
function won't return until the reference has been unlocked.
*/
/***************************************************************************/
RefObj::~RefObj()
{
   KillRef();
}

/**
  Assign a name to this reference.  The name is used for debugging purposes.
  @param name Pointer to the name.  Note that a local copy of this pointer will
              be stored in the reference object.
*/
void RefObj::SetRefName( const char *name )
{
   this->name = name;

#ifdef ADD_NAMES_TO_REFINFO
   MutexLocker ml( refTblMtx() );
   RefInfo *ri = GetRefInfoPtr( refID );
   CML_ASSERT( ri );
   ri->name = name;
#endif
}

/***************************************************************************/
/**
Destroy this reference.  This function should be called at the beginning of 
the destructor of any object that inherits from RefObj.  It removes this 
reference from the system and delays until no other threads are actively 
using a pointer to the referenced object.

This should be the first thing done in a destructor of any class that inherits 
from a RefObj, even if it inherits indirectly from the reference.  When KillRef
is called, the reference class makes sure that no other thread is holding a
lock on the class.  This prevents accidental object deletion while an object is
still in use by another thread.
*/
/***************************************************************************/
void RefObj::KillRef( void )
{
   int32 myID = refID;

   // If the id is negative, then this reference is already dead
   // This is perfectly normal since this function should be called
   // at the beginning of any destructor of a class based on the
   // RefObj.
   if( refID < 0 ) return;

   refTblMtx().Lock();

   // Get the info associated with this reference.
   RefInfo *ri = GetRefInfoPtr( myID );

   // Make sure we found the info structure.  This
   // should never fail.
   CML_ASSERT( ri );
   if( !ri )
   {
      refTblMtx().Unlock();
      return;
   }

   // Clear the pointer stored in this info structure.
   // This prevents any other thread from locking the
   // reference from this point on.
   ri->ptr = 0;

   // Wait until this object is no longer locked
   // by any other thread.
   for( int i=0; (i<2000) && ri->lockCt; i++ )
   {
      refTblMtx().Unlock();
      Thread::sleep(1);
      refTblMtx().Lock();
   }

   // If this fails it's a pretty serious error.  It means that some other 
   // object is still holding a pointer to this object and we are being
   // distroyed.
   bool fail = (ri->lockCt > 0);
   if( fail )
      cml.Error( "Timeout waiting on release of reference 0x%08x (%p), name %s\n", myID, this, (name==0) ? "none" : name );

   ReleaseRefInfo( ri, myID );
   refID = -1;

   refTblMtx().Unlock();
}

/***************************************************************************/
/**
This function is used to enable or disable the autodelete function 
for a reference object.  

If automatic deletion is enabled, the object will be deleted automatically
when it's reference count indicates that there are no other objects in 
the system which are still holding a reference to it.

Obviously, this should only be enabled for objects that have been allocated
from the heap using the new operator.

@param autoDeleteEna Boolean that if is true enabled the auto delete
*/
/***************************************************************************/
void RefObj::setAutoDelete( bool autoDeleteEna )
{
   // Get the info associated with this reference.
   RefInfo *ri = GetRefInfoPtr( refID );
   if( !ri ) return;

   ri->autoDeleteEna = autoDeleteEna;
}

/***************************************************************************/
/**
Grab a reference to this object.  This function increases the reference count
associated with the object.  The reference returned can be safely used to lock
and unlock the associated object.

For each call to GrabRef, there should be a corresponding call to 
RefObj::ReleaseRef to release the reference when it's no longer needed.

@return The object referece, or 0 if it wasn't possible to grab a reference
to the object.
*/
/***************************************************************************/
uint32 RefObj::GrabRef( void )
{
   MutexLocker ml( refTblMtx() );

   RefInfo *ri = GetRefInfoPtr( refID );
   if( !ri ) return 0;

   ri->refCt++;
   return refID+1;
}

/***************************************************************************/
/**
Release the local reference to this object.

@param val The reference previously returned by a call to RefObj::GrabRef
 */
/***************************************************************************/
void RefObj::ReleaseRef( uint32 val )
{
   // Find my internal ID value associated with this reference.
   int32 refID = val-1;

   MutexLocker ml( refTblMtx() );

   // Find the reference info structure.  This will return null
   // if an invalid reference number was passed
   RefInfo *ri = GetRefInfoPtr( refID );
   if( !ri ) return;

   ReleaseRefInfo( ri, refID );
   
   // If auto delete is enabled, then check
   if ( (ri->autoDeleteEna) && ri->refCt<=1 )
   {
      delete ri->ptr;
   }
}

/***************************************************************************/
/**
Find the object associated with the passed reference number and lock it to 
prevent the object from being destroyed while I'm accessing it.  The lock 
should only be held for a short time because it can prevent other threads
from deleting the object.  Call RefObj::Unlock when finished accessing 
the object.

@param val The reference ID associated with the object
@return A pointer to the referenced object if it still exists, or NULL if it's
        been destroyed.
 */
/***************************************************************************/
RefObj *RefObj::LockRef( uint32 val )
{
   int32 refID = val-1;

   MutexLocker ml( refTblMtx() );

   // Find my local info about this reference 
   RefInfo *ri = GetRefInfoPtr( refID );

   // Just return null if the reference is no longer valid
   if( !ri ) return 0;

   // Make sure the lock count isn't negative.  If it is, 
   // then this reference isn't valid (the ref info is on 
   // my recycle list)
   CML_ASSERT( ri->lockCt >= 0 );
   if( ri->lockCt < 0 )
      return 0;

   // Lock the reference and return a pointer to the object
   // if the object still exists in the system.
   if( ri->ptr )
      ri->lockCt++;

   return ri->ptr;
}

/***************************************************************************/
/**
Unlock an object that was previously locked using RefObj::Lock.
*/
/***************************************************************************/
void RefObj::UnlockRef( void )
{
   MutexLocker ml( refTblMtx() );

   // Find my local info about this reference 
   RefInfo *ri = GetRefInfoPtr( refID );

   CML_ASSERT( ri );
   if( !ri ) return;

   // Make sure the reference is actually locked
   CML_ASSERT( ri->lockCt > 0 );
   if( ri->lockCt <= 0 ) return;

   ri->lockCt--;
}

/**
 * Find the reference info structure corresponding to this 
 * ID value.
 *
 * The reference info structures are stored in a set of 
 * dynamically allocated arrays.  Each array holds 2^n 
 * structures, and there are 2^m arrays max.  Both n and m
 * must be integers.
 *
 * The lowest n bits of the ID give the index into an array,
 * and the next m bits give the array number.
 *
 * The local mutex should be locked when this function is called.
 */
static RefInfo *GetRefInfoPtr( int32 id )
{
   // If the ID is negative, then the reference is invalid
   if( id < 0 ) return 0;

   // If the ID is greater >= my max allocted reference, then
   // it's invalid.
   if( id >= nextNewRef ) return 0;

   // Find the block number for this reference and make sure it's valid
   int32 blk = id>>CML_REF_BITS;
   if( blk >= totAllocBlocks )
      return 0;

   // Find the offset within the block
   int32 ndx = id & (REF_PER_BLOCK-1);

   RefInfo *tbl = refBlocks[blk];
   return &tbl[ ndx ];
}

static void ReleaseRefInfo( RefInfo *ri, int32 id )
{
   // Make sure the reference info is sane.  There must be at least
   // one reference.
   CML_ASSERT( ri->refCt > 0 );
   ri->refCt--;

   if( !ri->refCt )
   {
      // Make sure the reference isn't locked (shouldn't happen)
      // If this assertion is hit it means that the parent object of this reference
      // is being destroyed while some other object still has it locked.
      // No object should lock a reference and keep it locked for a long time.
      // When a decendent of the reference class is destroyed it waits for any locks
      // to be released before calling this function.  If this delay times out 
      // then this assertion will be hit.
      CML_ASSERT( ri->lockCt == 0 );

      // Add this to my list of recycled info structures.
      ri->next = recycleList;
      ri->lockCt = -1;
      recycleList = id;
   }
}

/**
  This function is provided for debugging.  It prints out information on all references 
  that are currently held to the cml.log file
*/
void RefObj::LogRefs( void )
{
   cml.Debug( "List of outstanding references.  Total allocated %d\n", nextNewRef );

   int32 refNum = 1;

   for( int i=0; i<totAllocBlocks; i++ )
   {
      RefInfo *blk = refBlocks[i];
      for( int j=0; (j<REF_PER_BLOCK) && (refNum<nextNewRef); j++, refNum++ )
      {
         // If the lock count is < 0, the reference is on my recycle list.  I don't print these
         if( blk[j].lockCt < 0 )
            continue;

         RefObj *ptr = blk[j].ptr;

         const char *riName = "";
#ifdef ADD_NAMES_TO_REFINFO
         if( blk[j].name )
            riName = blk[j].name;
#endif

         // If the pointer is zero, then the object has been deleted, but there are outstanding references to it.
         if( !ptr )
            cml.Debug( " - Reference 0x%08x (%s) has %d open references, but the reference is no longer valid.\n", refNum, riName, blk[j].refCt );

         // Otherwise, the reference is still live
         else
            cml.Debug( " - Reference 0x%08x has %d open references.  Attached to pointer %p, name %s\n", refNum, blk[j].refCt, ptr, ptr->name );
      }
   }
   cml.Debug( "End of reference list\n" );
}

