/*
 * Copyright (c) 2007-2008 Digital Bazaar, Inc.  All rights reserved.
 */
#include "db/net/http/HttpConnection.h"
#include "db/net/http/HttpRequest.h"
#include "db/net/http/HttpResponse.h"
#include "db/net/http/HttpBodyOutputStream.h"
#include "db/net/http/HttpChunkedTransferInputStream.h"
#include "db/net/http/HttpChunkedTransferOutputStream.h"
#include "db/rt/Thread.h"

using namespace std;
using namespace db::io;
using namespace db::net;
using namespace db::net::http;
using namespace db::rt;

const unsigned long long MAX_ULONG_VALUE = 0xffffffffffffffffLL;
const unsigned long HALF_MAX_LONG_VALUE =
   (unsigned long)(MAX_ULONG_VALUE / 2);

HttpConnection::HttpConnection(Connection* c, bool cleanup) :
   WebConnection(c, cleanup)
{
   // no content bytes read yet
   mContentBytesRead = 0;
   
   // no content bytes written yet
   mContentBytesWritten = 0;
}

HttpConnection::~HttpConnection()
{
}

WebRequest* HttpConnection::createRequest()
{
   // create HttpRequest
   return new HttpRequest(this);
}

IOException* HttpConnection::sendHeader(HttpHeader* header)
{
   IOException* rval = NULL;
   
   string out;
   header->toString(out);
   if(!getOutputStream()->write(out.c_str(), out.length()))
   {
      rval = (IOException*)Exception::getLast();
   }
   
   return rval;
}

IOException* HttpConnection::receiveHeader(HttpHeader* header)
{
   IOException* rval = NULL;
   
   // read until eof, error, or blank line w/CRLF
   string headerStr;
   string line;
   ConnectionInputStream* is = getInputStream();
   Exception::setLast(NULL);
   while(is->readCrlf(line) && line.length() > 0)
   {
      headerStr.append(line);
      headerStr.append(HttpHeader::CRLF);
   }
   
   if(Exception::getLast() != NULL)
   {
      rval = (IOException*)Exception::getLast();
   }
   else
   {
      // parse header
      if(!header->parse(headerStr))
      {
         rval = new IOException(
            "Could not receive HTTP header!", "db.net.http.BadRequest");
         Exception::setLast(rval);
      }
   }
   
   return rval;
}

IOException* HttpConnection::sendBody(
   HttpHeader* header, InputStream* is, HttpTrailer* trailer)
{
   IOException* rval = NULL;
   
   // create HttpBodyOutputStream
   HttpBodyOutputStream os(this, header, trailer);
   
   // determine how much content needs to be read
   long long contentLength = 0;
   bool lengthUnspecified = true;
   if(header->getField("Content-Length", contentLength) && contentLength > 0)
   {
      lengthUnspecified = false;
   }
   
   // vars for read/write
   bool writeError = false;
   unsigned int length = 2048;
   char b[length];
   int numBytes = 0;
   
   // do unspecified length transfer
   if(lengthUnspecified)
   {
      // read in content, write out to connection
      while(!writeError && (numBytes = is->read(b, length)) > 0)
      {
         // write out to connection
         writeError = !os.write(b, numBytes);
      }
      
      if(!writeError)
      {
         // close stream
         os.close();
      }
   }
   else
   {
      // do specified length transfer:
      
      // read in content, write out to connection
      unsigned long long contentRemaining = contentLength;
      unsigned int readSize = (contentRemaining < length) ?
         contentRemaining : length;
      while(!writeError && contentRemaining > 0 &&
            (numBytes = is->read(b, readSize)) > 0)
      {
         // write out to connection
         if(os.write(b, numBytes))
         {
            contentRemaining -= numBytes;
            readSize = (contentRemaining < length) ? contentRemaining : length;
         }
         else
         {
            writeError = true;
         }
      }
      
      // check to see if an error occurred: if a write error occurred or
      // if content is remaining
      if(writeError)
      {
         rval = (IOException*)Exception::getLast();
      }
      else
      {
         if(contentRemaining > 0)
         {
            Thread* t = Thread::currentThread();
            if(t->isInterrupted())
            {
               // we will probably want this to be more robust in the
               // future so this kind of exception can be recovered from
               rval = new IOException(
                  "Sending HTTP content body interrupted!");
            }
            else
            {
               rval = new IOException(
                  "Could not read HTTP content bytes to send!");
            }
            
            Exception::setLast(rval);
         }
         else
         {
            // close stream
            os.close();
         }
      }
   }
   
   return rval;
}

OutputStream* HttpConnection::getBodyOutputStream(
   HttpHeader* header, HttpTrailer* trailer)
{
   return new HttpBodyOutputStream(this, header, trailer);
}

IOException* HttpConnection::receiveBody(
   HttpHeader* header, OutputStream* os, HttpTrailer* trailer)
{
   IOException* rval = NULL;
   
   InputStream* is = getInputStream();
   
   // wrap input stream if using chunked transfer encoding
   HttpChunkedTransferInputStream* chunkin = NULL;
   string transferEncoding;
   if(header->getField("Transfer-Encoding", transferEncoding))
   {
      if(strncasecmp(transferEncoding.c_str(), "chunked", 7) == 0)
      {
         is = chunkin = new HttpChunkedTransferInputStream(
            getInputStream(), trailer);
      }
   }
   
   // determine how much content needs to be received
   long long contentLength = 0;
   bool lengthUnspecified = true;
   if(header->getField("Content-Length", contentLength) && contentLength > 0)
   {
      lengthUnspecified = false;
   }
   
   // vars for read/write
   bool writeError = false;
   unsigned int length = 2048;
   char b[length];
   int numBytes = 0;
   
   // do chunked or unspecified length transfer
   if(chunkin != NULL || lengthUnspecified)
   {
      // read in from connection, write out content
      while(!writeError && (numBytes = is->read(b, length)) > 0)
      {
         // update http connection content bytes read (reset as necessary)
         if(getContentBytesRead() > HALF_MAX_LONG_VALUE)
         {
            setContentBytesRead(0);
         }
         
         setContentBytesRead(getContentBytesRead() + numBytes);
         
         // write out content
         writeError = !os->write(b, numBytes);
      }
      
      if(chunkin != NULL)
      {
         // clean up chunkin
         chunkin->close();
         delete chunkin;
      }
   }
   else
   {
      // do specified length transfer:
      
      // read in from connection, write out content
      unsigned long long contentRemaining = contentLength;
      unsigned int readSize = (contentRemaining < length) ?
         contentRemaining : length;
      while(!writeError && contentRemaining > 0 &&
            (numBytes = is->read(b, readSize)) > 0)
      {
         contentRemaining -= numBytes;
         readSize = (contentRemaining < length) ? contentRemaining : length;
         
         // update http connection content bytes read (reset as necessary)
         if(getContentBytesRead() > HALF_MAX_LONG_VALUE)
         {
            setContentBytesRead(0);
         }
         
         setContentBytesRead(getContentBytesRead() + numBytes);
         
         // write out content
         writeError = !os->write(b, numBytes);
      }
      
      // check to see if an error occurred: if a write error occurred or
      // if content is remaining
      if(writeError)
      {
         rval = (IOException*)Exception::getLast();
      }
      else if(contentRemaining > 0)
      {
         Thread* t = Thread::currentThread();
         if(t->isInterrupted())
         {
            // we will probably want this to be more robust in the
            // future so this kind of exception can be recovered from
            rval = new IOException(
               "Receiving HTTP content body interrupted!");
         }
         else
         {
            rval = new IOException(
               "Could not receive all HTTP content bytes!");
         }
         
         Exception::setLast(rval);
      }
   }
   
   return rval;
}

void HttpConnection::setContentBytesRead(unsigned long long count)
{
   mContentBytesRead = count;
}

unsigned long long HttpConnection::getContentBytesRead()
{
   return mContentBytesRead;
}

void HttpConnection::setContentBytesWritten(unsigned long long count)
{
   mContentBytesWritten = count;
}

unsigned long long HttpConnection::getContentBytesWritten()
{
   return mContentBytesWritten;
}
