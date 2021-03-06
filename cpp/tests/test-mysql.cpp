/*
 * Copyright (c) 2007-2010 Digital Bazaar, Inc. All rights reserved.
 */
#define __STDC_FORMAT_MACROS

#include "monarch/test/Test.h"
#include "monarch/test/TestModule.h"
#include "monarch/rt/Thread.h"
#include "monarch/sql/Row.h"
#include "monarch/sql/StatementBuilder.h"
#include "monarch/sql/mysql/MySqlConnection.h"
#include "monarch/sql/mysql/MySqlConnectionPool.h"
#include "monarch/sql/mysql/MySqlDatabaseClient.h"
#include "monarch/util/Timer.h"

#include <cstdio>

// You must create this file in order for the tests to work. Please
// see the file named "test-mysql.h.template" in the current directory
// for details
#include "monarch/tests/test-mysql.h"

using namespace std;
using namespace monarch::test;
using namespace monarch::rt;
using namespace monarch::sql;
using namespace monarch::sql::mysql;
using namespace monarch::util;

#define TABLE_TEST "test.momysqltest"

namespace mo_test_mysql
{

static void createMySqlTable(TestRunner& tr, monarch::sql::Connection* c)
{
   tr.test("drop table");
   {
      monarch::sql::Statement* s = c->prepare(
         "DROP TABLE IF EXISTS " TABLE_TEST);
      assertNoExceptionSet();
      s->execute();
   }
   tr.passIfNoException();

   tr.test("create table");
   {
      monarch::sql::Statement* s = c->prepare(
         "CREATE TABLE IF NOT EXISTS " TABLE_TEST
         " (id BIGINT AUTO_INCREMENT, t TEXT, i BIGINT, "
         "PRIMARY KEY (id))");
      assertNoExceptionSet();
      s->execute();
   }
   tr.passIfNoException();
}

static void executeMySqlStatements(TestRunner& tr, monarch::sql::Connection* c)
{
   tr.test("insert test 1");
   {
      monarch::sql::Statement* s = c->prepare(
         "INSERT INTO " TABLE_TEST " (t, i) VALUES ('test!', 1234)");
      assertNoExceptionSet();
      s->execute();
      assert(s->getLastInsertRowId() > 0);
   }
   tr.passIfNoException();

   tr.test("insert test 2");
   {
      monarch::sql::Statement* s = c->prepare(
         "INSERT INTO " TABLE_TEST " (t, i) VALUES ('!tset', 4321)");
      assertNoExceptionSet();
      s->execute();
      assert(s->getLastInsertRowId() > 0);
   }
   tr.passIfNoException();

   tr.test("insert positional parameters test");
   {
      monarch::sql::Statement* s;
      //uint64_t start = System::getCurrentMilliseconds();
      for(int i = 0; i < 20; ++i)
      {
         s = c->prepare("INSERT INTO " TABLE_TEST " (t, i) VALUES (?, ?)");
         assertNoExceptionSet();
         s->setText(1, "boundpositional");
         s->setInt32(2, 2220 + i);
         s->execute();
         assert(s->getLastInsertRowId() > 0);
         assertNoExceptionSet();
      }
      //uint64_t end = System::getCurrentMilliseconds();
      //printf("TIME=%" PRIu64 " ms\n", (end - start));
   }
   tr.passIfNoException();

   tr.test("select test");
   {
      monarch::sql::Statement* s = c->prepare("SELECT t, i FROM " TABLE_TEST);
      assertNoExceptionSet();
      s->execute();
      assertNoExceptionSet();

      // fetch rows
      Row* row;
      string t;
      int i;
      while((row = s->fetch()) != NULL)
      {
         row->getText("t", t);
         assertNoExceptionSet();
         row->getInt32("i", i);
         assertNoExceptionSet();

         if(strcmp(t.c_str(), "test!") == 0)
         {
            assert(i == 1234);
         }
         else if(strcmp(t.c_str(), "!tset") == 0)
         {
            assert(i == 4321);
         }
         else if(strcmp(t.c_str(), "boundpositional") == 0)
         {
            assert(i >= 2220);
         }
         else
         {
            // bad row data
            printf("BAD ROW DATA: %s\n", t.c_str());
            assert(false);
         }
      }
   }
   tr.passIfNoException();

   tr.test("select command ordering test");
   {
      monarch::sql::Statement* s = c->prepare("SELECT t, i FROM " TABLE_TEST);
      assertNoExceptionSet();
      s->execute();
      assertNoExceptionSet();

      // fetch rows
      Row* row;
      string t;
      int i;
      while((row = s->fetch()) != NULL)
      {
         row->getText("t", t);
         assertNoExceptionSet();
         row->getInt32("i", i);
         assertNoExceptionSet();

         if(strcmp(t.c_str(), "test!") == 0)
         {
            assert(i == 1234);
         }
         else if(strcmp(t.c_str(), "!tset") == 0)
         {
            assert(i == 4321);
         }
         else if(strcmp(t.c_str(), "boundpositional") == 0)
         {
            assert(i >= 2220);
         }
         else
         {
            // bad row data
            assert(false);
         }
      }
   }
   tr.passIfNoException();
}

static void runMySqlConnectionTest(TestRunner& tr)
{
   tr.test("MySql Connection");

   MySqlConnection c;
   c.connect("mysql://" MYSQL_READ_USER ":" MYSQL_PASSWORD "@"
      MYSQL_HOST "/test");
   c.close();
   assertNoExceptionSet();

   // clean up mysql
   mysql_library_end();

   tr.pass();
}

static void runMySqlStatementTest(TestRunner& tr)
{
   tr.group("MySql Statement");

   // clear any exceptions
   Exception::clear();

   MySqlConnection c;
   c.connect("mysql://" MYSQL_WRITE_USER ":" MYSQL_PASSWORD "@" MYSQL_HOST);
   assertNoExceptionSet();

   // create table
   createMySqlTable(tr, &c);

   // execute mysql statements
   executeMySqlStatements(tr, &c);

   tr.test("connection close");
   {
      c.close();
   }
   tr.passIfNoException();

   // clean up mysql
   mysql_library_end();

   tr.ungroup();
}

static void runMySqlDatabaseClientTest(TestRunner& tr)
{
   tr.group("DatabaseClient");

   // create mysql connection pools
   ConnectionPoolRef readPool = new MySqlConnectionPool(
      "mysql://" MYSQL_READ_USER ":" MYSQL_PASSWORD "@" MYSQL_HOST, 1);
   ConnectionPoolRef writePool = new MySqlConnectionPool(
      "mysql://" MYSQL_WRITE_USER ":" MYSQL_PASSWORD "@" MYSQL_HOST, 1);
   assertNoExceptionSet();

   // create database client
   DatabaseClientRef dbc = new MySqlDatabaseClient();
   dbc->setDebugLogging(true);
   dbc->setReadConnectionPool(readPool);
   dbc->setWriteConnectionPool(writePool);

   tr.test("initialize");
   {
      dbc->initialize();
   }
   tr.passIfNoException();

   tr.test("define table");
   {
      SchemaObject schema;
      schema["table"] = TABLE_TEST;
      schema["indices"]->append("PRIMARY KEY(foo_id)");

      // stored in object as string, in database as uint64
      DatabaseClient::addSchemaColumn(schema,
         "foo_id", "BIGINT(20) UNSIGNED AUTO_INCREMENT",
         "fooId", String, UInt64);
      DatabaseClient::addSchemaColumn(schema,
         "foo_string", "TEXT", "fooString", String);
      DatabaseClient::addSchemaColumn(schema,
         "foo_flag", "TINYINT(1) UNSIGNED", "fooFlag", Boolean);
      DatabaseClient::addSchemaColumn(schema,
         "foo_int32", "TINYINT(1) UNSIGNED", "fooInt32", Int32);
      DatabaseClient::addSchemaColumn(schema,
         "foo_blob", "BLOB", "fooHex", String);
      schema["columns"].last()["encode"]->append("hex");

      dbc->define(schema);
   }
   tr.passIfNoException();

   tr.test("drop table if exists");
   {
      dbc->drop(TABLE_TEST, true);
   }
   tr.passIfNoException();

   tr.test("create table");
   {
      dbc->create(TABLE_TEST, false);
   }
   tr.passIfNoException();

   tr.test("create table if not exists");
   {
      dbc->create(TABLE_TEST, true);
   }
   tr.passIfNoException();

   tr.test("insert");
   {
      DynamicObject row;
      row["fooString"] = "foobar";
      row["fooFlag"] = true;
      row["fooInt32"] = 3;
      row["fooHex"] = "4a";
      SqlExecutableRef se = dbc->insert(TABLE_TEST, row);
      dbc->execute(se);
      assertNoExceptionSet();
      row["fooId"] = se->lastInsertRowId;
      row["fooId"]->setType(String);

      DynamicObject expect;
      expect["fooId"] = "1";
      expect["fooString"] = "foobar";
      expect["fooFlag"] = true;
      expect["fooInt32"] = 3;
      expect["fooHex"] = "4a";
      if(expect != row)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(row);
      }
      assert(expect == row);
   }
   tr.passIfNoException();

   tr.test("insert again");
   {
      DynamicObject row;
      row["fooString"] = "foobar";
      row["fooFlag"] = false;
      row["fooInt32"] = 3;
      row["fooHex"] = "4a";
      SqlExecutableRef se = dbc->insert(TABLE_TEST, row);
      dbc->execute(se);
      assertNoExceptionSet();
      row["fooId"] = se->lastInsertRowId;
      row["fooId"]->setType(String);

      DynamicObject expect;
      expect["fooId"] = "2";
      expect["fooString"] = "foobar";
      expect["fooFlag"] = false;
      expect["fooInt32"] = 3;
      expect["fooHex"] = "4a";
      if(expect != row)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(row);
      }
      assert(expect == row);
   }
   tr.passIfNoException();

   tr.test("select one");
   {
      DynamicObject where;
      where["fooId"] = "1";
      SqlExecutableRef se = dbc->selectOne(TABLE_TEST, &where);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect["fooId"] = "1";
      expect["fooString"] = "foobar";
      expect["fooFlag"] = true;
      expect["fooInt32"] = 3;
      expect["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("select one specific member");
   {
      DynamicObject where;
      where["fooId"] = "1";
      DynamicObject members;
      members["fooString"];
      SqlExecutableRef se = dbc->selectOne(TABLE_TEST, &where, &members);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect["fooString"] = "foobar";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("select");
   {
      DynamicObject where;
      where["fooInt32"] = 3;
      SqlExecutableRef se = dbc->select(TABLE_TEST, &where, NULL, 5);
      assert(!se.isNull());
      se->returnRowsFound = true;
      dbc->execute(se);
      assertNoExceptionSet();
      assert(se->rowsFound == 2);

      DynamicObject expect;
      expect->setType(Array);
      DynamicObject& first = expect->append();
      first["fooId"] = "1";
      first["fooString"] = "foobar";
      first["fooFlag"] = true;
      first["fooInt32"] = 3;
      first["fooHex"] = "4a";
      DynamicObject& second = expect->append();
      second["fooId"] = "2";
      second["fooString"] = "foobar";
      second["fooFlag"] = false;
      second["fooInt32"] = 3;
      second["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("update");
   {
      DynamicObject row;
      row["fooString"] = "foobar2";
      DynamicObject where;
      where["fooId"] = "2";
      SqlExecutableRef se = dbc->update(TABLE_TEST, row, &where);
      dbc->execute(se);
      assert(se->rowsAffected = 1);
   }
   tr.passIfNoException();

   tr.test("update w/limit");
   {
      DynamicObject row;
      row["fooString"] = "bar";
      DynamicObject where;
      where["fooId"] = "2";
      SqlExecutableRef se = dbc->update(TABLE_TEST, row, &where, 1);
      dbc->execute(se);
      assert(se->rowsAffected = 1);
   }
   tr.passIfNoException();

   tr.test("select updated one");
   {
      DynamicObject where;
      where["fooString"] = "bar";
      SqlExecutableRef se = dbc->selectOne(TABLE_TEST, &where);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect["fooId"] = "2";
      expect["fooString"] = "bar";
      expect["fooFlag"] = false;
      expect["fooInt32"] = 3;
      expect["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("select updated");
   {
      DynamicObject where;
      where["fooString"] = "bar";
      SqlExecutableRef se = dbc->select(TABLE_TEST, &where);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect[0]["fooId"] = "2";
      expect[0]["fooString"] = "bar";
      expect[0]["fooFlag"] = false;
      expect[0]["fooInt32"] = 3;
      expect[0]["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("select IN()");
   {
      DynamicObject where;
      where["fooString"]->append("bar");
      where["fooString"]->append("foobar");
      SqlExecutableRef se = dbc->select(TABLE_TEST, &where);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect->setType(Array);
      DynamicObject& first = expect->append();
      first["fooId"] = "1";
      first["fooString"] = "foobar";
      first["fooFlag"] = true;
      first["fooInt32"] = 3;
      first["fooHex"] = "4a";
      DynamicObject& second = expect->append();
      second["fooId"] = "2";
      second["fooString"] = "bar";
      second["fooFlag"] = false;
      second["fooInt32"] = 3;
      second["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("select range");
   {
      DynamicObject where;
      where["fooString"][0]["op"] = ">=";
      where["fooString"][0]["value"] = "a";
      where["fooString"][1]["op"] = "<=";
      where["fooString"][1]["value"] = "z";
      SqlExecutableRef se = dbc->select(TABLE_TEST, &where);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect->setType(Array);
      DynamicObject& first = expect->append();
      first["fooId"] = "1";
      first["fooString"] = "foobar";
      first["fooFlag"] = true;
      first["fooInt32"] = 3;
      first["fooHex"] = "4a";
      DynamicObject& second = expect->append();
      second["fooId"] = "2";
      second["fooString"] = "bar";
      second["fooFlag"] = false;
      second["fooInt32"] = 3;
      second["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("insert on duplicate key update");
   {
      DynamicObject row;
      row["fooId"] = "1";
      row["fooString"] = "duplicate key update";
      SqlExecutableRef se = dbc->insertOnDuplicateKeyUpdate(TABLE_TEST, row);
      dbc->execute(se);
      assert(se->rowsAffected = 1);
   }
   tr.passIfNoException();

   tr.test("select duplicate key updated");
   {
      DynamicObject where;
      where["fooString"] = "duplicate key update";
      SqlExecutableRef se = dbc->selectOne(TABLE_TEST, &where);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect["fooId"] = "1";
      expect["fooString"] = "duplicate key update";
      expect["fooFlag"] = true;
      expect["fooInt32"] = 3;
      expect["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.test("remove w/limit");
   {
      DynamicObject where;
      where["fooId"] = "1";
      SqlExecutableRef se = dbc->remove(TABLE_TEST, &where, 1);
      dbc->execute(se);
      assert(se->rowsAffected == 1);
   }
   tr.passIfNoException();

   tr.test("select again");
   {
      SqlExecutableRef se = dbc->select(TABLE_TEST);
      dbc->execute(se);
      assertNoExceptionSet();

      DynamicObject expect;
      expect[0]["fooId"] = "2";
      expect[0]["fooString"] = "bar";
      expect[0]["fooFlag"] = false;
      expect[0]["fooInt32"] = 3;
      expect[0]["fooHex"] = "4a";
      if(expect != se->result)
      {
         printf("expected:\n");
         dumpDynamicObject(expect);
         printf("got:\n");
         dumpDynamicObject(se->result);
      }
      assert(expect == se->result);
   }
   tr.passIfNoException();

   tr.ungroup();
}

class MySqlConnectionPoolTest : public Runnable
{
public:
   MySqlConnectionPool* pool;
   TestRunner* tr;

   virtual void run()
   {
      monarch::sql::Connection* c = pool->getConnection();
      executeMySqlStatements(*tr, c);
      c->close();
   }
};

static void runMySqlConnectionPoolTest(TestRunner& tr)
{
   tr.group("MySql ConnectionPool");

   // create mysql connection pool
   MySqlConnectionPool cp(
      "mysql://" MYSQL_WRITE_USER ":" MYSQL_PASSWORD "@" MYSQL_HOST, 100);
   assertNoExceptionSet();

   // create table
   monarch::sql::Connection* c = cp.getConnection();
   createMySqlTable(tr, c);
   c->close();

   // create connection test threads
   int testCount = 300;
   MySqlConnectionPoolTest* tests = new MySqlConnectionPoolTest[testCount];
   Thread* threads[testCount];

   // create threads, set pool for tests
   for(int i = 0; i < testCount; ++i)
   {
      tests[i].pool = &cp;
      tests[i].tr = &tr;
      threads[i] = new Thread(&tests[i]);
   }

   uint64_t startTime = Timer::startTiming();

   // run connection threads
   for(int i = 0; i < testCount; ++i)
   {
      while(!threads[i]->start(131072))
      {
         threads[i - 1]->join();
      }
   }

   // join threads
   for(int i = 0; i < testCount; ++i)
   {
      threads[i]->join();
   }

   double seconds = Timer::getSeconds(startTime);

   // clean up threads
   for(int i = 0; i < testCount; ++i)
   {
      delete threads[i];
   }

   delete[] tests;

   // clean up mysql
   mysql_library_end();

   // print report
   printf("\nNumber of independent connection uses: %d\n", testCount);
   printf("Number of pooled connections created: %d\n",
      cp.getConnectionCount());
   printf("Total time: %g seconds\n", seconds);

   tr.ungroup();
}

static void runMySqlStatementBuilderTest(TestRunner& tr)
{
   tr.group("MySql StatementBuilder");

   /* ObjRelMap: {} of
    *    "objectType": object-type
    *    "members": {} of
    *       "member-name": {} of
    *          "group": "columns" or "fkeys" (the group the mapping is for)
    *          "table": database table name
    *          "column": database column name
    *          "memberType": object member type
    *          "columnType": database column type
    *          "ftable": if group="fkeys", foreign key database table
    *          "fkey": if group="fkeys", foreign key database key column
    *          "fcolumn": if group="fkeys", foreign key database value column
    */

   // create mysql connection pools
   ConnectionPoolRef readPool = new MySqlConnectionPool(
      "mysql://" MYSQL_READ_USER ":" MYSQL_PASSWORD "@" MYSQL_HOST, 1);
   ConnectionPoolRef writePool = new MySqlConnectionPool(
      "mysql://" MYSQL_WRITE_USER ":" MYSQL_PASSWORD "@" MYSQL_HOST, 1);
   assertNoExceptionSet();

   // create database client
   DatabaseClientRef dbc = new MySqlDatabaseClient();
   dbc->setDebugLogging(true);
   dbc->setReadConnectionPool(readPool);
   dbc->setWriteConnectionPool(writePool);
   dbc->initialize();
   assertNoExceptionSet();

   // define an object type
   tr.test("set OR map");
   {
      ObjRelMap orMap;
      orMap["objectType"] = "Test";

      // define the object's members
      DynamicObject& members = orMap["members"];

      // id column
      {
         DynamicObject& entry = members["id"];
         entry["group"] = "columns";
         entry["table"] = TABLE_TEST;
         entry["column"] = "id";
         entry["columnType"]->setType(UInt64);
         entry["memberType"]->setType(String);
      }

      // t column
      {
         DynamicObject& entry = members["description"];
         entry["group"] = "columns";
         entry["table"] = TABLE_TEST;
         entry["column"] = "t";
         entry["columnType"]->setType(String);
         entry["memberType"]->setType(String);
      }

      // i column
      {
         DynamicObject& entry = members["number"];
         entry["group"] = "columns";
         entry["table"] = TABLE_TEST;
         entry["column"] = "i";
         entry["columnType"]->setType(UInt64);
         entry["memberType"]->setType(UInt64);
      }

      dbc->setObjRelMap(orMap);
   }
   tr.passIfNoException();

   monarch::sql::Connection* c = dbc->getWriteConnection();
   createMySqlTable(tr, c);

   tr.test("add Test object");
   {
      DynamicObject testObj;
      testObj["id"] = "123";
      testObj["description"] = "My test object description";
      testObj["number"] = 10;

      StatementBuilderRef sb = dbc->createStatementBuilder();
      sb->add("Test", testObj)->execute(c);
   }
   tr.passIfNoException();

   c->close();

   // clean up mysql
   mysql_library_end();

   tr.ungroup();
}

static bool run(TestRunner& tr)
{
   if(tr.isDefaultEnabled())
   {
      runMySqlConnectionTest(tr);
      runMySqlStatementTest(tr);
      runMySqlDatabaseClientTest(tr);
   }
   if(tr.isTestEnabled("mysql-connection-pool"))
   {
      runMySqlConnectionPoolTest(tr);
   }
   if(tr.isTestEnabled("mysql-statement-builder"))
   {
      runMySqlStatementBuilderTest(tr);
   }
   return true;
}

} // end namespace

MO_TEST_MODULE_FN("monarch.tests.mysql.test", "1.0", mo_test_mysql::run)
