/*
 * Copyright (c) 2009 Digital Bazaar, Inc. All rights reserved.
 */
#ifndef db_sql_DatabaseClient_H
#define db_sql_DatabaseClient_H

#include "db/sql/ConnectionPool.h"
#include "db/rt/DynamicObject.h"
#include "db/validation/Validation.h"

namespace db
{
namespace sql
{

/**
 * A SchemaObject contains the schema for a database table. It is used
 * to create a table and as a mapping between objects that make up the
 * input and output of database functions and the actual table schema
 * in the database. The schema defines which column names map to which
 * attributes in an input/output object.
 * 
 * SchemaObject: {} of
 *   "table": "tableName",
 *   "columns": [] of
 *     "column_name": {} of
 *       "type": "DATABASE COLUMN TYPE" (same as used in CREATE TABLE SQL),
 *       "memberName": "columnName" (member name as used in an object)
 */
typedef db::rt::DynamicObject SchemaObject;

/**
 * A DatabaseClient provides a simple interface to a database. The interface
 * abstracts away SQL and the connection and fetching APIs from its user,
 * removing much of the verbosity required to do basic database interaction.
 * 
 * The interface is largely object based/driven. Schema objects must be
 * provided to the DatabaseClient to initialize its use with various tables
 * in a database and its interaction with objects that may not store the
 * data in a given table using the same column names as its own attributes.
 * 
 * For instance, an object may have an attribute of "fooId" but the column
 * name in a related table may be "foo_id".
 * 
 * @author Dave Longley
 */
class DatabaseClient
{
protected:
   /**
    * A database read connection pool.
    */
   ConnectionPoolRef mReadPool;
   
   /**
    * A database write connection pool.
    */
   ConnectionPoolRef mWritePool;
   
   /**
    * Stores all schema objects, accessible via their table name.
    */
   db::rt::DynamicObject mSchemas;
   
   /**
    * Stores the schema validator.
    */
   db::validation::ValidatorRef mSchemaValidator;
   
public:
   /**
    * Creates a new DatabaseClient.
    */
   DatabaseClient();
   
   /**
    * Destructs this DatabaseClient.
    */
   virtual ~DatabaseClient();
   
   /**
    * Initializes this DatabaseClient.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool initialize();
   
   /**
    * Sets a connection pool to draw read connections from.
    * 
    * @param pool the read connection pool to use.
    */
   virtual void setReadConnectionPool(ConnectionPoolRef& pool);
   
   /**
    * Sets a connection pool to draw write connections from.
    * 
    * @param pool the write connection pool to use.
    */
   virtual void setWriteConnectionPool(ConnectionPoolRef& pool);
   
   /**
    * Gets a read connection from the read pool.
    * 
    * @return a Connection or NULL if a connection could not be made.
    */
   virtual db::sql::Connection* getReadConnection();
   
   /**
    * Gets a write connection from the write pool.
    * 
    * @return a Connection or NULL if a connection could not be made.
    */
   virtual db::sql::Connection* getWriteConnection();
   
   /**
    * Defines the schema for a table. This will not do CREATE TABLE, it
    * will only make interfacing with the given table possible via the
    * methods on this object. Call create() to attempt to create the table.
    * 
    * @param schema the schema for a table.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool define(SchemaObject& schema);
   
   /**
    * Creates a table via CREATE TABLE. The schema for the table must have
    * been previously set with defineTable.
    * 
    * @param table the name of the table to create.
    * @param ignoreIfExists true to ignore any errors if the table already
    *           exists, false to report them.
    * @param c the connection to use, NULL to obtain one from the pool. 
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool create(
      const char* table, bool ignoreIfExists, Connection* c = NULL);
   
   /**
    * Selects all column values not present in the given row object from
    * the specified table, using any present values in the WHERE clause
    * of the SELECT. 
    * 
    * @param table the name of the table to select from.
    * @param row the object to store the row result in, will be set to NULL
    *           if the SELECT returns back no rows.
    * @param c the connection to use, NULL to obtain one from the pool.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool selectOne(
      const char* table, db::rt::DynamicObject& row, Connection* c = NULL);
   
   /**
    * Selects all column values not present in the given WHERE object from
    * the specified table, using any present values in the WHERE clause
    * of the SELECT. Each row result will be stored in the array rows. An
    * optional LIMIT amount may be specified.
    * 
    * @param table the name of the table to SELECT FROM.
    * @param rows the array object to store the rows result in.
    * @param where an object that specifies specific column values to
    *           look for, NULL to include no WHERE clause.
    * @param limit 0 for no LIMIT, something positive to specify a LIMIT.
    * @param start the starting row for the LIMIT, defaults to 0.
    * @param c the connection to use, NULL to obtain one from the pool.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool select(
      const char* table, db::rt::DynamicObject& rows,
      db::rt::DynamicObject* where = NULL,
      uint64_t limit = 0, uint64_t start = 0,
      Connection* c = NULL);
   
   /**
    * Inserts a row into a table. All applicable values in the given object
    * will be inserted into the given table, according to its schema.
    * 
    * @param table the name of the table to INSERT INTO.
    * @param row the object with data to insert as a row.
    * @param c the connection to use, NULL to obtain one from the pool.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool insert(
      const char* table, db::rt::DynamicObject& row, Connection* c = NULL);
   
   /**
    * Updates a row in a table. All applicable values in the given object
    * will be updated in the given table, according to its schema. If the
    * given "where" object is not NULL, its applicable members will define
    * the WHERE clause of the UPDATE SQL. An optional LIMIT amount may be
    * specified.
    * 
    * @param table the name of the table to UPDATE.
    * @param row the object with data to use in the update.
    * @param where the object with containing WHERE clause parameters.
    * @param limit 0 for no LIMIT, something positive to specify a LIMIT.
    * @param start the starting row for the LIMIT, defaults to 0.
    * @param c the connection to use, NULL to obtain one from the pool.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool update(
      const char* table, db::rt::DynamicObject& row,
      db::rt::DynamicObject* where,
      uint64_t limit = 0, uint64_t start = 0,
      Connection* c = NULL);
   
   /**
    * Removes rows from a table. If the given "where" object is not NULL, its
    * applicable members will define the WHERE clause of the UPDATE SQL.
    * 
    * @param table the name of the table to DELETE FROM.
    * @param where the object with containing WHERE clause parameters.
    * @param c the connection to use, NULL to obtain one from the pool.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool remove(
      const char* table, db::rt::DynamicObject* where, Connection* c = NULL);
   
   /**
    * Begins a database transaction.
    * 
    * @param c the connection to use.
    */
   virtual bool begin(Connection* c);
   
   /**
    * Ends a transaction either with a COMMIT or a ROLLBACK. Which is used
    * is specified by the given boolean parameter.
    * 
    * @param c the connection to use.
    * @param commit true to commit the transaction, false to roll it back.
    * 
    * @return true if successful, false if an Exception occurred.
    */
   virtual bool end(Connection* c, bool commit);
};

} // end namespace sql
} // end namespace db
#endif