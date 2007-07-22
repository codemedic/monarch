/*
 * Copyright (c) 2007 Digital Bazaar, Inc.  All rights reserved.
 */
#include "OperationList.h"

using namespace std;
using namespace db::modest;

OperationList::OperationList(bool cleanup)
{
   mCleanup = cleanup;
}

OperationList::~OperationList()
{
   terminate();
}

void OperationList::add(Operation* op)
{
   lock();
   {
      mOperations.push_back(op);
   }
   unlock();
}

void OperationList::interrupt()
{
   lock();
   {
      for(list<Operation*>::iterator i = mOperations.begin();
          i != mOperations.end(); i++)
      {
         (*i)->interrupt();
      }
   }
   unlock();
}

void OperationList::waitFor()
{
   lock();
   {
      for(list<Operation*>::iterator i = mOperations.begin();
          i != mOperations.end(); i++)
      {
         (*i)->waitFor();
      }
   }
   unlock();
}

void OperationList::prune()
{
   lock();
   {
      for(list<Operation*>::iterator i = mOperations.begin();
          i != mOperations.end();)
      {
         if((*i)->finished() || (*i)->canceled())
         {
            if(mCleanup)
            {
               // reclaim memory if clean up flag is set
               delete *i;
            }
            
            // remove operation from list
            i = mOperations.erase(i);
         }
         else
         {
            i++;
         }
      }
   }
   unlock();
}

void OperationList::terminate()
{
   lock();
   {
      interrupt();
      waitFor();
      prune();
   }
   unlock();
}
