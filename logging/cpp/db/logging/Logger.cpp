/*
 * Copyright (c) 2007-2008 Digital Bazaar, Inc.  All rights reserved.
 */

#include "db/data/json/JsonWriter.h"
#include "db/logging/Logger.h"
#include "db/io/OStreamOutputStream.h"
#include "db/util/Date.h"
#include "db/rt/Thread.h"

#include <iostream>
#include <sstream>
#include <cstdlib>

using namespace std;
using namespace db::data::json;
using namespace db::io;
using namespace db::util;
using namespace db::logging;
using namespace db::rt;

// DO NOT INITIALIZE THIS VARIABLE! Logger::sLoggers is not not initialized on 
// purpose due to compiler initialization code issues.
Logger::LoggerMap* Logger::sLoggers;

Logger::Logger() :
   mDateFormat(NULL)
{
   setLevel(Max);
   setDateFormat("%Y-%m-%d %H:%M:%S");
   setFlags(LogDefaultFlags);
}

Logger::~Logger()
{
   if(mDateFormat != NULL)
   {
      free(mDateFormat);
   }
}

void Logger::initialize()
{
   // Create the global map of loggers
   sLoggers = new LoggerMap();
}

void Logger::cleanup()
{
   delete sLoggers;
   sLoggers = NULL;
}

/**
 * Map to convert log-level option names to Logger::Level types
 */
struct logLevelMap {
   const char* key;
   Logger::Level level;
};
static const struct logLevelMap logLevelsMap[] = {
   {"n", Logger::None},
   {"none", Logger::None},
   {"e", Logger::Error},
   {"error", Logger::Error},
   {"w", Logger::Warning},
   {"warning", Logger::Warning},
   {"i", Logger::Info},
   {"info", Logger::Info},
   {"d", Logger::Debug},
   {"debug", Logger::Debug},
   {"debug-data", Logger::DebugData},
   {"debug-detail", Logger::DebugDetail},
   {"m", Logger::Max},
   {"max", Logger::Max},
   {NULL, Logger::None}
};

bool Logger::stringToLevel(const char* slevel, Level& level)
{
   bool found = false;
   for(int mapi = 0;
      slevel != NULL&& !found && logLevelsMap[mapi].key != NULL;
      mapi++)
   {
      if(strcasecmp(slevel, logLevelsMap[mapi].key) == 0)
      {
         level = logLevelsMap[mapi].level;
         found = true;
      }
   }
   
   return found;
}

const char* Logger::levelToString(Level level)
{
   const char* rval;

   switch(level)
   {
      case None:
         rval = "NONE";
         break;
      case Error:
         rval = "ERROR";
         break;
      case Warning:
         rval = "WARNING";
         break;
      case Info:
         rval = "INFO";
         break;
      case Debug:
         rval = "DEBUG";
         break;
      case DebugData:
         rval = "DEBUG-DATA";
         break;
      case DebugDetail:
         rval = "DEBUG-DETAIL";
         break;
      case Max:
         rval = "MAX";
         break;
      default:
         rval = NULL;
   }

   return rval;
}

void Logger::addLogger(Logger* logger, Category* category)
{
   if(sLoggers != NULL)
   {
      sLoggers->insert(pair<Category*, Logger*>(category, logger));
   }
}

void Logger::removeLogger(Logger* logger, Category* category)
{
   if(sLoggers != NULL)
   {
      // FIX ME: We need to iterate through, we can't do a find()
      LoggerMap::iterator i = sLoggers->find(category);
      if(i != sLoggers->end())
      {
         LoggerMap::iterator end = sLoggers->upper_bound(category);
         for(; i != end; i++)
         {
            if(logger == i->second)
            {
               sLoggers->erase(i);
               break;
            }
         }
      }
   }
}

void Logger::clearLoggers()
{
   if(sLoggers != NULL)
   {
      sLoggers->clear();
   }
}

void Logger::setLevel(Level level)
{
   mLevel = level;
}

Logger::Level Logger::getLevel()
{
   return mLevel;
}

void Logger::getDate(string& date)
{
   date.erase();
   
   if(strcmp(mDateFormat, "") == 0)
   {
      // shortcut - do nothing
   }
   else
   {
      // handle other date formats here
      Date now;
      date = now.format(date, mDateFormat);
   }
}

bool Logger::setDateFormat(const char* format)
{
   // lock so flags are not changed while in use in log()
   lock();
   {
      if(mDateFormat != NULL)
      {
         free(mDateFormat);
      }
      
      mDateFormat = strdup(format);
   }
   unlock();

   return true;
}

void Logger::setFlags(LoggerFlags flags)
{
   // lock so flags are not changed while in use in log()
   lock();
   {
      mFlags = flags;
   }
   unlock();
}

Logger::LoggerFlags Logger::getFlags()
{
   return mFlags;
}

/**
 * Adapted from glibc sprintf docs.
 */
char* Logger::makeMessage(const char *format, va_list varargs)
{
   /* Guess we need no more than 128 bytes. */
   int n, size = 128;
   char *p, *np;

   if ((p = (char*)malloc(size)) == NULL)
   {
      return NULL;
   }

   while (1)
   {
      /* Try to print in the allocated space. */
      n = vsnprintf(p, size, format, varargs);
      /* If that worked, return the string. */
      if (n > -1 && n < size)
      {
         return p;
      }
      /* Else try again with more space. */
      if (n > -1)    /* glibc 2.1 */
         size = n+1; /* precisely what is needed */
      else           /* glibc 2.0 */
         size *= 2;  /* twice the old size */
      if ((np = (char*)realloc (p, size)) == NULL) {
         free(p);
         return NULL;
      } else {
         p = np;
      }
   }
}

bool Logger::log(
   Category* cat,
   Level level,
   const char* location,
   const void* object,
   LogFlags flags,
   const char* format,
   va_list varargs)
{
   bool rval = false;
   
   if(mLevel >= level)
   {
      lock();

      // Output fields depending on flags as:
      // [date: ][thread ][object ][level ][cat ][location ]message

      string logText;
      
      if(mFlags & LogDate)
      {
         string date;
         getDate(date);
         if(strcmp(date.c_str(), "") != 0)
         {
            logText.append(date);
            logText.push_back(' ');
         }
      }

      if(mFlags & LogThread)
      {
         Thread* thread = Thread::currentThread();
         const char* name = thread->getName();
         if(name)
         {
            logText.append(name);
         }
         else
         {
            char address[23];
            snprintf(address, 23, "%p", thread);
            logText.append(address);
         }
         logText.push_back(' ');
      }

      if((mFlags & LogObject) && (flags & LogObjectValid))
      {
         if(object)
         {
            char address[23];
            snprintf(address, 23, "%p", object);
            logText.append(address);
         }
         else
         {
            // force 0x0 rather than "(nil)" from %p format string
            logText.append("0x0");
         }
         logText.push_back(' ');
      }

      if(mFlags & LogLevel)
      {
         logText.append(levelToString(level));
         logText.push_back(' ');
      }

      if((mFlags & LogCategory) && cat)
      {
         // FIXME: add flag to select name type
         // Try id if set, else try name.
         const char* name = cat->getId();
         name = name ? name : cat->getName();
         if(name)
         {
            logText.append(name);
            logText.push_back(' ');
         }
      }

      if((mFlags & LogLocation) && location)
      {
         logText.append(location);
         logText.push_back(' ');
      }

      char* message = makeMessage(format, varargs);
      if(message)
      {
         logText.append(message);
         free(message);
      }
      logText.push_back('\n');
      
      log(logText.c_str());
      rval = true;

      unlock();
   }

   return rval;
}

bool Logger::log(
   Category* cat,
   Level level,
   const char* location,
   const void* object,
   LogFlags flags,
   const char* format,
   ...)
{
   bool rval;
   
   va_list varargs;
   va_start(varargs, format);
   rval = log(cat, level, location, object, flags, format, varargs);
   va_end(varargs);
   
   return rval;
}


void Logger::logToLoggers(
   Category* registeredCat,
   Category* messageCat,
   Level level,
   const char* location,
   const void* object,
   LogFlags flags,
   const char* format,
   va_list varargs)
{
   if(sLoggers != NULL)
   {
      // Find this category
      LoggerMap::iterator i = sLoggers->find(registeredCat);
      if(i != sLoggers->end())
      {
         // Find the last logger in this category
         LoggerMap::iterator end = sLoggers->upper_bound(registeredCat);
         for(; i != end; i++)
         {
            // Log the message
            Logger* logger = i->second;
            logger->log(
               messageCat, level, location, object, flags, format, varargs);
         }
      }
   }
}

void Logger::logToLoggers(
   Category* registeredCat,
   Category* messageCat,
   Level level,
   const char* location,
   const void* object,
   LogFlags flags,
   const char* format,
   ...)
{
   va_list varargs;
   va_start(varargs, format);
   logToLoggers(registeredCat, messageCat,
      level, location, object, flags, format, varargs);
   va_end(varargs);
}

void Logger::logToLoggers(
   Category* cat,
   Level level,
   const char* location,
   const void* object,
   LogFlags flags,
   const char* format,
   va_list varargs)
{
   // Log to loggers registered for this category
   logToLoggers(cat, cat, level, location, object, flags, format, varargs);
   // Log to loggers registered for all categories
   logToLoggers(
      DB_ALL_CAT, cat, level, location, object, flags, format, varargs);
}

void Logger::logToLoggers(
   Category* cat,
   Level level,
   const char* location,
   const void* object,
   LogFlags flags,
   const char* format,
   ...)
{
   va_list varargs;
   va_start(varargs, format);
   logToLoggers(cat, level, location, object, flags, format, varargs);
   va_end(varargs);
}
