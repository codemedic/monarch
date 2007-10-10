/*
 * Copyright (c) 2007 Digital Bazaar, Inc.  All rights reserved.
 */
#include "db/net/http/HttpRequestHeader.h"

using namespace std;
using namespace db::net;
using namespace db::net::http;
using namespace db::util;

HttpRequestHeader::HttpRequestHeader()
{
   mMethod = strdup("");
   mVersion = strdup("");
   mPath = strdup("");
}

HttpRequestHeader::~HttpRequestHeader()
{
   delete [] mMethod;
   delete [] mVersion;
   delete [] mPath;
}

bool HttpRequestHeader::parseStartLine(const char* str, unsigned int length)
{
   // copy string so it can be modified
   char tokens[length + 1];
   strncpy(tokens, str, length);
   memset(tokens + length, 0, 1);
   
   // find space-delimited tokens in the passed string
   int count = 0;
   char* start = tokens;
   char* end;
   while(start != NULL && count < 3)
   {
      // find the end of the token
      end = strchr(start, ' ');
      if(end != NULL)
      {
         // nullify delimiter
         memset(end, 0, 1);
      }
      
      switch(count++)
      {
         case 0:
            setMethod(start);
            break;
         case 1:
            setPath(start);
            break;
         case 2:
            setVersion(start);
            break;
      }
      
      if(end != NULL)
      {
         start = end + 1;
      }
      else
      {
         start = end;
      }
   }
   
   return count == 3;
}

void HttpRequestHeader::getStartLine(string& line)
{
   line.append(getMethod());
   line.append(1, ' ');
   line.append(getPath());
   line.append(1, ' ');
   line.append(getVersion());
}

void HttpRequestHeader::setMethod(const char* method)
{
   delete [] mMethod;
   mMethod = strdup(method);
}

const char* HttpRequestHeader::getMethod()
{
   return mMethod;
}

void HttpRequestHeader::setVersion(const char* version)
{
   delete [] mVersion;
   mVersion = strdup(version);
}

const char* HttpRequestHeader::getVersion()
{
   return mVersion;
}

void HttpRequestHeader::setPath(const char* path)
{
   delete [] mPath;
   mPath = strdup(path);
}

const char* HttpRequestHeader::getPath()
{
   return mPath;
}
