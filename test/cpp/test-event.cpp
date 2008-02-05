/*
 * Copyright (c) 2007-2008 Digital Bazaar, Inc.  All rights reserved.
 */
#include <iostream>
#include <sstream>

#include "db/test/Test.h"
#include "db/test/Tester.h"
#include "db/test/TestRunner.h"
#include "db/util/DynamicObject.h"
#include "db/util/DynamicObjectIterator.h"
#include "db/modest/Kernel.h"
#include "db/event/Observable.h"
#include "db/event/ObserverDelegate.h"
#include "db/event/EventController.h"

using namespace std;
using namespace db::test;
using namespace db::event;
using namespace db::modest;
using namespace db::rt;
using namespace db::util;

class TestObserver : public Observer
{
public:
   int events;
   int event1;
   int event2;
   int event3;
   int event4;
   
   ObserverDelegate<TestObserver> delegate1;
   ObserverDelegate<TestObserver> delegate2;
   ObserverDelegate<TestObserver> delegate3;
   ObserverDelegate<TestObserver> delegate4;
   
   TestObserver() :
      delegate1(this, &TestObserver::handleEvent1),
      delegate2(this, &TestObserver::handleEvent2),
      delegate3(this, &TestObserver::handleEvent3),
      delegate4(this, &TestObserver::handleEvent4)
   {
      events = 0;
      event1 = 0;
      event2 = 0;
      event3 = 0;
      event4 = 0;
   }
   
   virtual ~TestObserver()
   {
   }
   
   virtual void eventOccurred(Event& e)
   {
      events++;
   }
   
   virtual void handleEvent1(Event& e)
   {
      event1++;
   }
   
   virtual void handleEvent2(Event& e)
   {
      event2++;
   }
   
   virtual void handleEvent3(Event& e)
   {
      event3++;
   }
   
   virtual void handleEvent4(Event& e)
   {
      if(e["id"]->getUInt64() == 3)
      {
         event3++;
      }
      else if(e["id"]->getUInt64() == 4)
      {
         event4++;
      }
   }
};

void runEventTest(TestRunner& tr)
{
   tr.test("Event");
   
   // create kernel and start engine
   Kernel k;
   k.getEngine()->start();
   
   // create observable and observer
   Observable observable;
   TestObserver observer;
   
   // register observer and start observable
   observable.registerObserver(&observer, 1);
   observable.start(&k);
   
   // create and schedule events
   Event e1;
   Event e2;
   Event e3;
   e1["name"] = "Event1";
   e2["name"] = "Event2";
   e3["name"] = "Event3";
   observable.schedule(e1, 1);
   observable.schedule(e2, 1);
   observable.schedule(e3, 1);
   
   // wait for a second
   Thread::sleep(1000);
   
   assert(observer.events == 3);
   
   // stop observable
   observable.stop();
   
   // stop kernel engine
   k.getEngine()->stop();
   
   tr.pass();
}

void runObserverDelegateTest(TestRunner& tr)
{
   tr.test("ObserverDelegate");
   
   // create kernel and start engine
   Kernel k;
   k.getEngine()->start();
   
   // create observable and observers
   Observable observable;
   TestObserver observer;
   
   // register observers and start observable
   observable.registerObserver(&observer.delegate1, 1);
   observable.registerObserver(&observer.delegate2, 2);
   observable.registerObserver(&observer.delegate3, 3);
   observable.registerObserver(&observer.delegate4, 4);
   observable.addTap(3, 4);
   observable.start(&k);
   
   // create and schedule events
   Event e1;
   Event e2;
   Event e3;
   Event e4;
   e1["name"] = "Event1";
   e2["name"] = "Event2";
   e3["name"] = "Event3";
   e4["name"] = "Event4";
   observable.schedule(e1, 1);
   observable.schedule(e2, 2);
   observable.schedule(e3, 3);
   observable.schedule(e4, 4);
   
   // wait for a second
   Thread::sleep(1000);
   
   assert(observer.event1 == 1);
   assert(observer.event2 == 1);
   assert(observer.event3 == 2);
   assert(observer.event4 == 1);
   
   // stop observable
   observable.stop();
   
   // stop kernel engine
   k.getEngine()->stop();
   
   tr.pass();
}

void runEventControllerTest(TestRunner& tr)
{
   tr.test("EventController");
   
   // create kernel and start engine
   Kernel k;
   k.getEngine()->start();
   
   // create event controller
   EventController ec;
   
   // create observers
   TestObserver observer;
   
   DynamicObject types;
   // string type
   ec.registerObserver(&observer.delegate1, "event1");
   // DynamicObject array of string types
   types[0] = "event2";
   ec.registerObserver(&observer.delegate2, types);
   types[0] = "event3";
   ec.registerObserver(&observer.delegate3, types);
   types[0] = "event4";
   ec.registerObserver(&observer.delegate4, types);
   
   types[0] = "event1";
   ec.registerObserver(&observer, types);
   
   // add parent events
   ec.addParent("event2", "event1");
   ec.addParent("event3", "event1");
   ec.addParent("event4", "event3");
   
   // start event controller
   ec.start(&k);
   
   // create and schedule events
   Event e1;
   Event e2;
   Event e3;
   Event e4;
   e1["type"] = "event1";
   e2["type"] = "event2";
   e3["type"] = "event3";
   e4["type"] = "event4";
   ec.schedule(e1);
   ec.schedule(e2);
   ec.schedule(e3);
   ec.schedule(e4);
   
   // wait for a second
   Thread::sleep(1000);
   
   // check messages
   assert(observer.events == 4);
   assert(observer.event1 == 4);
   assert(observer.event2 == 1);
   assert(observer.event3 == 2);
   assert(observer.event4 == 1);
   
   // stop event controller
   ec.stop();
   
   // stop kernel engine
   k.getEngine()->stop();
   
   tr.pass();
}

class DbEventTester : public db::test::Tester
{
public:
   DbEventTester()
   {
      setName("event");
   }

   /**
    * Run automatic unit tests.
    */
   virtual int runAutomaticTests(TestRunner& tr)
   {
      runEventTest(tr);
      runObserverDelegateTest(tr);
      runEventControllerTest(tr);
      return 0;
   }

   /**
    * Runs interactive unit tests.
    */
   virtual int runInteractiveTests(TestRunner& tr)
   {
      return 0;
   }
};

#ifndef DB_TEST_NO_MAIN
DB_TEST_MAIN(DbEventTester)
#endif