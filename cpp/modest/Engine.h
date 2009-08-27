/*
 * Copyright (c) 2007-2009 Digital Bazaar, Inc. All rights reserved.
 */
#ifndef db_modest_Engine_H
#define db_modest_Engine_H

#include "db/modest/State.h"
#include "db/modest/Operation.h"
#include "db/rt/ThreadPool.h"

namespace db
{
namespace modest
{

// forward declare OperationDispatcher
class OperationDispatcher;

/**
 * A Modest Engine (MODular Extensible State Engine) is a lightweight
 * processing engine that keeps track of state information and can be
 * extended by Modules that make use of existing functionality and
 * provide new functionality.
 *
 * The Modest Engine executes Operations to change its current State.
 * Operations may be executed concurrently.
 *
 * The Modest Engine is intended to be "modest" in its complexity and
 * code base, but powerful in its extensibility. The core engine provides
 * a cross-platform thread pool for executing Operations in an orderly
 * fashion. The design intends to allow developers to create Modules that
 * can concurrently run multiple Operations that must by synchronized with
 * one another in some fashion.
 *
 * @author Dave Longley
 */
class Engine
{
protected:
   /**
    * The State of this Engine.
    */
   State* mState;

   /**
    * The OperationDispatcher for dispatching Operations.
    */
   OperationDispatcher* mOpDispatcher;

   /**
    * A lock for starting/stopping this Engine.
    */
   db::rt::ExclusiveLock mLock;

public:
   /**
    * Creates a new Engine.
    */
   Engine();

   /**
    * Destructs this Engine.
    */
   virtual ~Engine();

   /**
    * Queues the passed Operation for execution. The Operation may fail to
    * execute if this Engine's State is not compatible with the Operation's
    * guard. The Operation may also be placed in a wait queue to be checked
    * later for execution.
    *
    * After this method has been called, the Operation may be waited on until
    * it finishes or is canceled, by calling op->waitFor().
    *
    * @param op the Operation to execute.
    */
   virtual void queue(Operation& op);

   /**
    * Starts this Engine. This will begin executing queued Operations.
    */
   virtual void start();

   /**
    * Stops this Engine. This will stop executing queued Operations and
    * interrupt all currently running Operations.
    */
   virtual void stop();

   /**
    * Gets State of this Engine in an immutable form.
    *
    * @return the State of this Engine in an immutable form.
    */
   virtual ImmutableState* getState();

   /**
    * Gets this Engine's ThreadPool.
    *
    * @return this Engine's ThreadPool.
    */
   virtual db::rt::ThreadPool* getThreadPool();

   /**
    * Gets this Engine's OperationDispatcher.
    *
    * @return this Engine's OperationDispatcher.
    */
   virtual OperationDispatcher* getOperationDispatcher();
};

} // end namespace modest
} // end namespace db
#endif