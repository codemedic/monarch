/*
 * Copyright (c) 2008 Digital Bazaar, Inc.  All rights reserved.
 */
#include "db/io/MutatorOutputStream.h"

using namespace db::io;
using namespace db::rt;

MutatorOutputStream::MutatorOutputStream(
   OutputStream* os, bool cleanupStream,
   MutationAlgorithm* algorithm, bool cleanupAlgorithm) :
   FilterOutputStream(os, cleanupStream),
   mSource(2048),
   mDestination(4096)
{
   // store mutation algorithm
   mAlgorithm = algorithm;
   mCleanupAlgorithm = cleanupAlgorithm;
   mResult = MutationAlgorithm::NeedsData;
}

MutatorOutputStream::~MutatorOutputStream()
{
   if(mCleanupAlgorithm && mAlgorithm != NULL)
   {
      delete mAlgorithm;
   }
}

bool MutatorOutputStream::write(const char* b, int length)
{
   bool rval = true;
   
   // determine buffer to obtain source data from
   ByteBuffer* src;
   if(!mSource.isEmpty())
   {
      // append passed data to the source buffer
      mSource.put(b, length, true);
      src = &mSource;
   }
   else
   {
      // wrap the passed data to mutated it without caching it
      mInputWrapper.setBytes((char*)b, 0, length, false);
      src = &mInputWrapper;
   }
   
   // signal algorithm to complete if length is zero
   bool finish = (length == 0);
   
   // keep mutating while algorithm does not need data and is not complete
   bool write = true;
   while(rval && write && mResult < MutationAlgorithm::CompleteAppend)
   {
      // try to mutate data
      mResult = mAlgorithm->mutateData(src, &mDestination, finish);
      switch(mResult)
      {
         case MutationAlgorithm::NeedsData:
            if(finish)
            {
               // no more data available for algorithm
               mResult = MutationAlgorithm::Error;
               Exception* e = new Exception(
                  "Insufficient data for mutation algorithm!",
                  "db.io.MutationException");
               Exception::setLast(e);
               rval = false;
            }
            else
            {
               // more data needed in order to write
               write = false;
            }
            break;
         case MutationAlgorithm::Error:
            // exception
            write = false;
            break;
         default:
            if(!mDestination.isEmpty())
            {
               // write destination data out
               rval = (mDestination.get(mOutputStream) > 0);
            }
            break;
      }
   }
   
   if(mResult == MutationAlgorithm::CompleteAppend)
   {
      if(!src->isEmpty())
      {
         // write source data to destination
         rval = (src->get(mOutputStream) > 0);
      }
   }
   else if(mResult == MutationAlgorithm::CompleteTruncate)
   {
      // clear any source bytes
      mSource.clear();
      mInputWrapper.clear();
   }
   else
   {
      // copy excess bytes into source buffer
      if(src == &mInputWrapper && !mInputWrapper.isEmpty())
      {
         mSource.put(&mInputWrapper, mInputWrapper.length(), true);
      }
   }
   
   return rval;
}

void MutatorOutputStream::close()
{
   // ensure mutation is finished
   write(NULL, 0);
   
   // close underlying stream
   mOutputStream->close();
}

void MutatorOutputStream::setAlgorithm(MutationAlgorithm* ma, bool cleanup)
{
   if(mCleanupAlgorithm && mAlgorithm != NULL)
   {
      delete mAlgorithm;
   }
   
   mAlgorithm = ma;
   mCleanupAlgorithm = cleanup;
   mResult = MutationAlgorithm::NeedsData;
}

MutationAlgorithm* MutatorOutputStream::getAlgorithm()
{
   return mAlgorithm;
}