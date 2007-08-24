/*
 * Copyright (c) 2007 Digital Bazaar, Inc.  All rights reserved.
 */
#ifndef db_data_xml_XmlBindingStreamOutputStream_H
#define db_data_xml_XmlBindingStreamOutputStream_H

#include "OutputStream.h"
#include "ByteArrayInputStream.h"
#include "xml/XmlReader.h"

namespace db
{
namespace data
{
namespace xml
{

/**
 * An XmlBindingOutputStream is used to write to xml a DataBinding.
 * 
 * @author Dave Longley
 */
class XmlBindingOutputStream : public db::io::OutputStream
{
protected:
   /**
    * The XmlReader to parse the data with.
    */
   XmlReader mReader;
   
   /**
    * A ByteArrayInputStream for reading the output to this stream.
    */
   db::io::ByteArrayInputStream mInputStream;
   
public:
   /**
    * Creates a new XmlBindingOutputStream that writes to the passed
    * DataBinding.
    * 
    * @param db the DataBinding to write to.
    */
   XmlBindingOutputStream(db::data::DataBinding* db);
   
   /**
    * Destructs this XmlBindingOutputStream.
    */
   virtual ~XmlBindingOutputStream();
   
   /**
    * Writes some bytes to the stream.
    * 
    * @param b the array of bytes to write.
    * @param length the number of bytes to write to the stream.
    * 
    * @return true if the write was successful, false if an IO exception
    *         occurred. 
    */
   virtual bool write(const char* b, unsigned int length);
   
   /**
    * Closes the stream.
    */
   virtual void close();
};

} // end namespace xml
} // end namespace data
} // end namespace db
#endif
