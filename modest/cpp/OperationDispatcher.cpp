/*
 * Copyright (c) 2007 Digital Bazaar, Inc.  All rights reserved.
 */
#include "OperationDispatcher.h"
#include "Engine.h"
#include "OperationExecutor.h"

using namespace std;
using namespace db::modest;
using namespace db::rt;

OperationDispatcher::OperationDispatcher(Engine* e)
{
   mEngine = e;
}

OperationDispatcher::~OperationDispatcher()
{
}

void OperationDispatcher::dispatchNextJob()
{
   OperationExecutor* e = NULL;
   
   // lock state, executor will unlock it
   mEngine->getState()->lock();
   
   lock();
   {
      // look up the queue until an Operation is found that can be executed
      for(list<Runnable*>::iterator i = mJobQueue.begin();
          e == NULL && i != mJobQueue.end();)
      {
         e = (OperationExecutor*)(*i);
         switch(e->checkEnvironment())
         {
            case 0:
               // Operation is executable
               i = mJobQueue.erase(i);
               break;
            case 1:
               // move to next Operation
               i++;
               e = NULL;
               break;
            case 2:
               // Operation is canceled
               i = mJobQueue.erase(i);
               e = NULL;
               break;
         }
      }
   }
   unlock();
   
   if(e != NULL)
   {
      // execute Operation
      e->execute();
   }
   else
   {
      // no executor, so unlock state
      mEngine->getState()->unlock();
   }
   
   // clean up any expired executors
   cleanupExpiredExecutors();
}

void OperationDispatcher::cleanupExpiredExecutors()
{
   lock();
   {
      for(list<OperationExecutor*>::iterator i = mExpiredExecutors.begin();
          i != mExpiredExecutors.end();)
      {
         OperationExecutor* e = *i;
         i = mExpiredExecutors.erase(i);
         delete e;
      }
   }
   unlock();
}

void OperationDispatcher::queueOperation(OperationExecutor* e)
{
   JobDispatcher::queueJob(e);
}

void OperationDispatcher::startDispatching()
{
   JobDispatcher::startDispatching();
}

void OperationDispatcher::stopDispatching()
{
   JobDispatcher::stopDispatching();
}

void OperationDispatcher::clearQueuedOperations()
{
   // synchronize
   lock();
   {
      // delete OperationExecutors in the queue
      for(list<Runnable*>::iterator i = mJobQueue.begin();
          i != mJobQueue.end();)
      {
         OperationExecutor* e = (OperationExecutor*)(*i);
         i = mJobQueue.erase(i);
         delete e;
      }
   }
   unlock();   
}

void OperationDispatcher::terminateRunningOperations()
{
   JobDispatcher::terminateAllRunningJobs();
}

void OperationDispatcher::addExpiredExecutor(OperationExecutor* e)
{
   lock();
   {
      mExpiredExecutors.push_back(e);
   }
   unlock();
}

JobThreadPool* OperationDispatcher::getThreadPool()
{
   return JobDispatcher::getThreadPool();
}

unsigned int OperationDispatcher::getQueuedOperationCount()
{
   return JobDispatcher::getQueuedJobCount();
}

unsigned int OperationDispatcher::getTotalOperationCount()
{
   return JobDispatcher::getTotalJobCount();
}
