/*
 * Copyright (c) 2007-2010 Digital Bazaar, Inc. All rights reserved.
 */
#include "monarch/data/xml/XmlReader.h"

using namespace std;
using namespace monarch::data;
using namespace monarch::data::xml;
using namespace monarch::io;
using namespace monarch::rt;

// initialize encoding
const char* XmlReader::CHAR_ENCODING = "UTF-8";

// initialize read size
unsigned int XmlReader::READ_SIZE = 4096;

// base exception name
#define XML_READER_EXCEPTION "monarch.data.xml.XmlReader"

XmlReader::XmlReader() :
   mStarted(false),
   mException(NULL)
{
}

XmlReader::~XmlReader()
{
   if(mStarted)
   {
      // free parser
      XML_ParserFree(mParser);
   }
}

bool XmlReader::start(DynamicObject& dyno)
{
   // clear stacks and push root object
   mDynoStack.clear();
   mTypeStack.clear();
   mDynoStack.push_front(&dyno);

   if(mStarted)
   {
      // free parser
      XML_ParserFree(mParser);
      mException.setNull();
   }

   // create parser
   mParser = XML_ParserCreateNS(CHAR_ENCODING, '|');

   // set user data to this reader
   XML_SetUserData(mParser, this);

   // set handlers
   XML_SetElementHandler(mParser, &startElement, &endElement);
   XML_SetCharacterDataHandler(mParser, &appendData);
   XML_SetNamespaceDeclHandler(
      mParser, &startNamespaceDeclaration, &endNamespaceDeclaration);

   // read started
   mStarted = true;
   return true;
}

bool XmlReader::read(InputStream* is)
{
   bool rval = true;

   if(!mStarted)
   {
      // reader not started
      ExceptionRef e = new Exception(
         "Cannot read yet, XmlReader not started.",
         XML_READER_EXCEPTION ".SetupError");
      Exception::set(e);
      rval = false;
   }
   else
   {
      int numBytes = 1;
      while(rval && numBytes > 0)
      {
         // get buffer from expat to store data
         char* b = (char*)XML_GetBuffer(mParser, READ_SIZE);
         if(b == NULL)
         {
            // set memory exception
            ExceptionRef e = new Exception(
               "Insufficient memory to parse xml.",
               XML_READER_EXCEPTION ".InsufficientMemory");
            Exception::set(e);
            rval = false;
         }
         else
         {
            // read data into buffer
            numBytes = is->read(b, READ_SIZE);
            if(numBytes > 0)
            {
               // parse data
               rval = (XML_ParseBuffer(mParser, numBytes, false) != 0);
            }

            if(!rval)
            {
               int line = XML_GetCurrentLineNumber(mParser);
               int column = XML_GetCurrentColumnNumber(mParser);
               const char* error = XML_ErrorString(XML_GetErrorCode(mParser));
               ExceptionRef e = new Exception(
                  "Xml parse error.",
                  XML_READER_EXCEPTION ".ParseError");
               e->getDetails()["line"] = line;
               e->getDetails()["column"] = column;
               e->getDetails()["error"] = error;
               Exception::set(e);
            }
            else if(numBytes == -1)
            {
               // input stream read error
               rval = false;
            }
         }
      }
   }

   return rval;
}

bool XmlReader::finish()
{
   bool rval = true;

   if(mStarted)
   {
      // parse last data
      if(XML_ParseBuffer(mParser, 0, true) == 0)
      {
         int line = XML_GetCurrentLineNumber(mParser);
         int column = XML_GetCurrentColumnNumber(mParser);
         const char* error = XML_ErrorString(XML_GetErrorCode(mParser));
         ExceptionRef e = new Exception(
            "Xml parse error.",
            XML_READER_EXCEPTION ".ParseError");
         e->getDetails()["line"] = line;
         e->getDetails()["column"] = column;
         e->getDetails()["error"] = error;
         Exception::set(e);
         rval = false;
      }

      // free parser
      XML_ParserFree(mParser);
   }

   // no longer started
   mStarted = false;

   return rval;
}

void XmlReader::startElement(const XML_Char* name, const XML_Char** attrs)
{
   if(mException.isNull() &&
      !mDynoStack.empty() && !mDynoStack.front()->isNull())
   {
      // parse element's local name
      parseLocalName(&name);

      if(strcmp(name, "member") == 0)
      {
         // create child object based on name
         bool set = false;
         for(int i = 0; !set && attrs[i] != NULL; i += 2)
         {
            // parse attribute's local name
            parseLocalName(&attrs[i]);

            // get object name
            if(strcmp(attrs[i], "name") == 0)
            {
               DynamicObject child;
               (*mDynoStack.front())[attrs[i + 1]] = child;
               mDynoStack.push_front(&(*mDynoStack.front())[attrs[i + 1]]);
               set = true;
            }
         }

         if(!set)
         {
            // no "name" for "member"
            mException = new Exception(
               "Xml parsing error. No 'name' attribute for 'member' element.",
               XML_READER_EXCEPTION ".MissingAttribute");
            Exception::set(mException);
         }
      }
      else if(strcmp(name, "element") == 0)
      {
         // create child object base on index
         bool set = false;
         for(int i = 0; !set && attrs[i] != NULL; i += 2)
         {
            // parse attribute's local name
            parseLocalName(&attrs[i]);

            // get object index
            if(strcmp(attrs[i], "index") == 0)
            {
               DynamicObject child;
               int index = strtoul(attrs[i + 1], NULL, 10);
               (*mDynoStack.front())[index] = child;
               mDynoStack.push_front(&(*mDynoStack.front())[index]);
               set = true;
            }
         }

         if(!set)
         {
            // no "name" for "member"
            mException = new Exception(
               "Xml parsing error. No 'index' attribute for 'element' "
               "element.",
               XML_READER_EXCEPTION ".MissingAttribute");
            Exception::set(mException);
         }
      }
      else
      {
         // determine object type from element name
         DynamicObjectType dot = tagNameToType(name);
         mTypeStack.push_front(dot);

         if(strcmp(name, "null") == 0)
         {
            // set dyno to null
            mDynoStack.front()->setNull();
         }
      }
   }
}

void XmlReader::endElement(const XML_Char* name)
{
   if(mException.isNull() && !mDynoStack.empty())
   {
      // parse element's local name
      parseLocalName(&name);

      if(mDynoStack.front()->isNull())
      {
         if(strcmp(name, "null") == 0)
         {
            // pop stacks
            mDynoStack.pop_front();
            mTypeStack.pop_front();
         }
      }
      else
      {
         // ensure name matches current dyno type
         DynamicObjectType dot = tagNameToType(name);
         if(dot == mTypeStack.front() &&
            (dot != String || strcmp(name, "string") == 0))
         {
            // parse number to appropriate type
            if(dot == UInt64)
            {
               const char* num = (*mDynoStack.front())->getString();
               if(strchr(num, '.') != NULL)
               {
                  // number has a decimal point
                  dot = Double;
               }
               else if(num[0] == '-')
               {
                  // number is signed
                  dot = Int64;
               }
            }

            // set object type
            (*mDynoStack.front())->setType(dot);

            // pop stacks
            mDynoStack.pop_front();
            mTypeStack.pop_front();
         }
      }
   }
}

void XmlReader::appendData(const XML_Char* data, int length)
{
   if(mException.isNull() &&
      !mDynoStack.empty() && !mDynoStack.front()->isNull())
   {
      // append data to dyno
      const char* d = (*mDynoStack.front())->getString();
      int len = strlen(d);
      char temp[len + length + 1];
      memcpy(temp, d, len);
      memcpy(temp + len, data, length);
      temp[len + length] = 0;
      *mDynoStack.front() = temp;
   }
}

void XmlReader::startNamespaceDeclaration(
   const XML_Char* prefix, const XML_Char* uri)
{
   // base class does nothing here
}

void XmlReader::endNamespaceDeclaration(const XML_Char* prefix)
{
   // base class does nothing here
}

void XmlReader::startElement(
   void* xr, const XML_Char* name, const XML_Char** attrs)
{
   // get reader, start element
   XmlReader* reader = (XmlReader*)xr;
   reader->startElement(name, attrs);
}

void XmlReader::endElement(void* xr, const XML_Char* name)
{
   // get reader, end element
   XmlReader* reader = (XmlReader*)xr;
   reader->endElement(name);
}

void XmlReader::appendData(void* xr, const XML_Char* data, int length)
{
   // get reader, append data
   XmlReader* reader = (XmlReader*)xr;
   reader->appendData(data, length);
}

void XmlReader::startNamespaceDeclaration(
   void* xr, const XML_Char* prefix, const XML_Char* uri)
{
   // get reader, call start namespace declaration
   XmlReader* reader = (XmlReader*)xr;
   reader->startNamespaceDeclaration(prefix, uri);
}

void XmlReader::endNamespaceDeclaration(void* xr, const XML_Char* prefix)
{
   // get reader, call end namespace declaration
   XmlReader* reader = (XmlReader*)xr;
   reader->endNamespaceDeclaration(prefix);
}

DynamicObjectType XmlReader::tagNameToType(const char* name)
{
   DynamicObjectType rval = String;

   // determine determine object type from name
   if(strcmp(name, "boolean") == 0)
   {
      rval = Boolean;
   }
   else if(strcmp(name, "number") == 0)
   {
      // default to largest integer type
      rval = UInt64;
   }
   else if(strcmp(name, "object") == 0)
   {
      rval = Map;
   }
   else if(strcmp(name, "array") == 0)
   {
      rval = Array;
   }

   return rval;
}

void XmlReader::parseLocalName(const char** fullName)
{
   // move pointer past namespace, if one exists
   const char* sep = strchr(*fullName, '|');
   if(sep != NULL)
   {
      *fullName = sep + 1;
   }
}

void XmlReader::parseNamespace(const char** fullName, char** ns)
{
   // parse namespace, if one exists
   *ns = NULL;
   const char* sep = strchr(*fullName, '|');
   if(sep != NULL)
   {
      *ns = (char*)malloc((sep - *fullName) + 1);
      strncpy(*ns, *fullName, sep - *fullName);
      (*ns)[(sep - *fullName)] = 0;
      *fullName = sep + 1;
   }
}
