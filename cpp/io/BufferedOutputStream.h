/*
 * Copyright (c) 2007-2009 Digital Bazaar, Inc. All rights reserved.
 */
#ifndef monarch_io_BufferedOutputStream_H
#define monarch_io_BufferedOutputStream_H

#include "monarch/io/FilterOutputStream.h"
#include "monarch/io/ByteBuffer.h"

namespace monarch
{
namespace io
{

/**
 * A BufferedOutputStream is an output stream that fills up a buffer before
 * flushing it to an underlying output stream.
 *
 * @author Dave Longley
 */
class BufferedOutputStream : public FilterOutputStream
{
protected:
   /**
    * The ByteBuffer to fill before flushing.
    */
   ByteBuffer* mBuffer;

public:
   /**
    * Creates a new BufferedOutputStream that uses the passed ByteBuffer and
    * writes to the passed OutputStream.
    *
    * The passed ByteBuffer should be sized to whatever amount (greater
    * than zero) is desired. It will not be resized.
    *
    * @param b the ByteBuffer to fill before flushing to the underlying stream.
    * @param os the OutputStream to write to.
    * @param cleanup true to clean up the passed OutputStream when destructing,
    *                false not to.
    */
   BufferedOutputStream(ByteBuffer* b, OutputStream* os, bool cleanup = false);

   /**
    * Destructs this BufferedOutputStream.
    */
   virtual ~BufferedOutputStream();

   /**
    * Writes some bytes to the stream.
    *
    * @param b the array of bytes to write.
    * @param length the number of bytes to write to the stream.
    *
    * @return true if the write was successful, false if an IO exception
    *         occurred.
    */
   virtual bool write(const char* b, int length);

   /**
    * Forces this stream to flush its output, if any of it was buffered.
    *
    * @return true if the write was successful, false if an IO exception
    *         occurred.
    */
   virtual bool flush();

   /**
    * Closes the stream.
    */
   virtual void close();

   /**
    * Sets the ByteBuffer to use.
    *
    * @param b the ByteBuffer to use.
    */
   virtual void setBuffer(ByteBuffer* b);
};

} // end namespace io
} // end namespace monarch
#endif
