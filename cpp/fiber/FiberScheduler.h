/*
 * Copyright (c) 2009 Digital Bazaar, Inc. All rights reserved.
 */
#ifndef monarch_fiber_FiberScheduler_H
#define monarch_fiber_FiberScheduler_H

#include "monarch/modest/OperationList.h"
#include "monarch/modest/OperationRunner.h"
#include "monarch/fiber/Fiber.h"

#include <map>

namespace monarch
{
namespace fiber
{

/**
 * A FiberScheduler schedules and runs Fibers. It uses N modest Operations to
 * run however many Fibers are assigned to it. Each Operation shares the same
 * FiberScheduler, calling it to acquire the next scheduled fiber to run each
 * time it finishes running a fiber.
 *
 * @author Dave Longley
 */
class FiberScheduler : public monarch::rt::Runnable
{
protected:
   /**
    * The list of Operations to run Fibers on.
    */
   monarch::modest::OperationList mOpList;

   /**
    * A list of scheduler contexts.
    */
   typedef std::list<FiberContext*> ContextList;
   ContextList mContextList;

   /**
    * The next fiber ID to try to assign.
    */
   FiberId mNextFiberId;

   /**
    * Set to true if map of fibers must be checked when assigning a new
    * fiber ID.
    */
   bool mCheckFiberMap;

   /**
    * A map of all fibers (fiber ID => fiber) in the scheduler.
    */
   typedef std::map<FiberId, Fiber*> FiberMap;
   FiberMap mFiberMap;

   /**
    * A queue of fibers to execute.
    */
   typedef std::list<Fiber*> FiberQueue;
   FiberQueue mFiberQueue;

   /**
    * A map of sleeping fibers.
    */
   FiberMap mSleepingFibers;

   /**
    * An exclusive lock for scheduling the next fiber.
    */
   monarch::rt::ExclusiveLock mScheduleLock;

   /**
    * An exclusive lock for waiting for the fiber list to empty.
    */
   monarch::rt::ExclusiveLock mNoFibersWaitLock;

public:
   /**
    * Creates a new FiberScheduler.
    */
   FiberScheduler();

   /**
    * Destructs this FiberScheduler.
    */
   virtual ~FiberScheduler();

   /**
    * Starts this FiberScheduler. It will create "numOps" Operations using
    * the passed OperationRunner to run its fibers on.
    *
    * @param opRunner the OperationRunner to use.
    * @param numOps the number of Operations to run fibers on.
    */
   virtual void start(
      monarch::modest::OperationRunner* opRunner, int numOps);

   /**
    * Stops this FiberScheduler. This method will not cause its Fibers to
    * exit, they will just no longer run.
    */
   virtual void stop();

   /**
    * Waits until all fibers have exited. This method will block the current
    * thread until all fibers have exited.
    *
    * @param stop true to stop the FiberScheduler when woken up (and not
    *             interrupted), false not to.
    *
    * @return true if all fibers exited, false if interrupted.
    */
   virtual bool waitForLastFiberExit(bool stop);

   /**
    * Adds a new fiber to this FiberScheduler. The passed fiber must be
    * heap-allocated. It will be deleted by this FiberScheduler when it
    * exits.
    *
    * @param fiber the fiber to add.
    *
    * @return the FiberId assigned to the fiber.
    */
   virtual FiberId addFiber(Fiber* fiber);

   /**
    * Yields the passed fiber. This *must* be called by a running fiber.
    *
    * @param fiber the fiber to field.
    */
   virtual void yield(Fiber* fiber);

   /**
    * Puts the passed fiber to sleep. This *must* be called by a running
    * fiber.
    *
    * @param fiber the fiber to put to sleep.
    */
   virtual void sleep(Fiber* fiber);

   /**
    * Wakes up this sleeping fiber. This *must* be called by the fiber that
    * is to be woken up.
    *
    * @param id the FiberId of the fiber to wakeup.
    */
   virtual void wakeupSelf(Fiber* fiber);

   /**
    * Wakes up any sleeping fiber. If the passed fiber ID has no associated
    * fiber, then this is a no-op.
    *
    * @param id the FiberId of the fiber to wakeup.
    */
   virtual void wakeup(FiberId id);

   /**
    * Exits the passed fiber, switching permanently its context out for
    * the current thread's scheduler context. This *must* be called by a
    * running fiber and is done so by default after its run() method exits.
    */
   virtual void exit(Fiber* fiber);

   /**
    * Runs this FiberScheduler. This method is executed inside of N modest
    * Operations.
    */
   virtual void run();

protected:
   /**
    * Gets the next schedulable fiber, if any.
    *
    * @return the next schedulable fiber, or NULL if none are available.
    */
   virtual Fiber* nextFiber();

   /**
    * Called to notify operations that a fiber is available to be scheduled.
    */
   virtual void fiberAvailable();

   /**
    * Called to notify that no fibers are in the system.
    */
   virtual void noFibersAvailable();
};

} // end namespace fiber
} // end namespace monarch
#endif
