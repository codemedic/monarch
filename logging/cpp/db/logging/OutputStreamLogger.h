/*
 * Copyright (c) 2007-2008 Digital Bazaar, Inc.  All rights reserved.
 */
#ifndef db_logging_OutputStreamLogger_H
#define db_logging_OutputStreamLogger_H

#include <map>

#include "db/logging/Logger.h"
#include "db/io/OutputStream.h"
#include "db/rt/ExclusiveLock.h"

namespace db
{
namespace logging
{

/**
 * A class that handles logging to an OutputStream.
 *  
 * @author Dave Longley
 * @author David I. Lehn
 * @author Manu Sporny
 */
class OutputStreamLogger : public db::logging::Logger
{
protected:
   /**
    * The file output stream to write logging information to.
    */
   db::io::OutputStream* mStream;
   
   /**
    * Flag to cleanup the output stream.
    */
   bool mCleanup;

   /**
    * Lock for critical sections.
    */
   db::rt::ExclusiveLock mLock;
   
public:
   /**
    * Creates a new logger with specified level.
    *
    * @param stream the stream to use.
    * @param cleanup if the stream should be cleaned up.
    */
   OutputStreamLogger(
      db::io::OutputStream* stream = NULL, bool cleanup = false);
   
   /**
    * Overloaded to ensure that the stream gets closed when garbage
    * collected.
    */
   virtual ~OutputStreamLogger();
   
   /**
    * Gets the print stream for this logger.
    * 
    * @return the print stream for this logger.
    */
   virtual db::io::OutputStream* getOutputStream();
   
   /**
    * Close and cleanup stream if cleanup flag set and stream exists.
    */
   virtual void close();

   /**
    * Sets the output stream.
    * 
    * @param os the output stream to use.
    * @param cleanup if the logger handles cleanup of this stream.
    * @param closeCurrent if the logger should close current stream.
    */
   virtual void setOutputStream(db::io::OutputStream* os, bool cleanup = false,
      bool closeCurrent = true);
   
   /**
    * Writes the message to the output stream.
    *
    * @param message the message to write to the output stream.
    * @param length length of message.
    */
   virtual void log(const char* message, size_t length);
};

} // end namespace logging
} // end namespace db
#endif
