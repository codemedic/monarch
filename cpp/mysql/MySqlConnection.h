/*
 * Copyright (c) 2007-2011 Digital Bazaar, Inc. All rights reserved.
 */
#ifndef monarch_sql_mysql_MySqlConnection_H
#define monarch_sql_mysql_MySqlConnection_H

#include <mysql/mysql.h>

#include "monarch/sql/AbstractConnection.h"

namespace monarch
{
namespace sql
{
namespace mysql
{

// forward declaration
class MySqlStatement;

/**
 * An MySqlConnection is a Connection to an mysql database.
 *
 * @author Dave Longley
 */
class MySqlConnection : public monarch::sql::AbstractConnection
{
protected:
   /**
    * The handle to the mysql database.
    */
   ::MYSQL* mHandle;

public:
   /**
    * Creates a new Connection.
    */
   MySqlConnection();

   /**
    * Destructs this Connection.
    */
   virtual ~MySqlConnection();

   /**
    * Gets the mysql connection handle.
    *
    * @return the mysql connection handle.
    */
   virtual ::MYSQL* getHandle();

   /**
    * Connects to the specified host using the specified default database. No
    * default database will be set if no path is provided in the url.
    *
    * @param url MySql parameters in URL form:
    *            "mysql://user:password@host:port/databasename"
    *
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool connect(monarch::util::Url* url);
   using monarch::sql::AbstractConnection::connect;

   /**
    * Closes this connection.
    */
   virtual void close();

   /**
    * Begins a new transaction.
    *
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool begin();

   /**
    * Commits the current transaction.
    *
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool commit();

   /**
    * Rolls back the current transaction.
    *
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool rollback();

   /**
    * Returns true if this connection is connected, false if not.
    *
    * @return true if this connection is connected, false if not.
    */
   virtual bool isConnected();

   /**
    * Sets the character set for this connection.
    *
    * @param cset the character set to use for this connection.
    *
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool setCharacterSet(const char* cset);

   /**
    * Performs a simple null-terminated query that returns no results. No
    * binary data permitted.
    *
    * @param sql the sql query to execute.
    *
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool query(const char* sql);

   /**
    * Sets the SQL mode on this connection.
    *
    * @param mode the mode to set.
    */
   virtual bool setSqlMode(const char* mode);

   /**
    * Sets the timezone for this connection.
    *
    * @param tz the timezone to set.
    */
   virtual bool setTimeZone(const char* tz);

   /**
    * Creates a new Exception using the last error set on this connection.
    *
    * @return the created Exception.
    */
   virtual monarch::rt::Exception* createException();

protected:
   /**
    * Creates a prepared Statement.
    *
    * @param sql the standard query language text of the Statement.
    *
    * @return the new Statement, NULL if an exception occurred.
    */
   virtual Statement* createStatement(const char* sql);
};

} // end namespace mysql
} // end namespace sql
} // end namespace monarch
#endif
