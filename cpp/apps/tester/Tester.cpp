/*
 * Copyright (c) 2008-2010 Digital Bazaar, Inc. All rights reserved.
 */
#include <cstdio>
#include <cstdlib>

#include "monarch/app/AppPluginFactory.h"
#include "monarch/kernel/MicroKernel.h"
#include "monarch/rt/Exception.h"
#include "monarch/test/Test.h"
#include "monarch/test/Testable.h"

#include "monarch/apps/tester/Tester.h"

using namespace std;
using namespace monarch::app;
using namespace monarch::apps::tester;
using namespace monarch::config;
using namespace monarch::kernel;
using namespace monarch::modest;
using namespace monarch::rt;
using namespace monarch::test;

#define PLUGIN_NAME "monarch.apps.tester.Tester"
#define PLUGIN_CL_CFG_ID PLUGIN_NAME ".commandLine"

Tester::Tester(MicroKernel* k) :
   mKernel(k)
{
}

Tester::~Tester()
{
}

bool Tester::initMetaConfig(Config& meta)
{
   bool rval = monarch::app::AppPlugin::initMetaConfig(meta);

   // defaults
   if(rval)
   {
      Config& c =
         App::makeMetaConfig(meta, PLUGIN_NAME ".defaults", "defaults")
            [ConfigManager::MERGE][PLUGIN_NAME];
      c["level"] = TestRunner::Names;
      c["continueAfterException"] = false;
      c["tests"]->setType(Map);
      c["modules"]->setType(Map);
   }

   // command line options
   if(rval)
   {
      Config& c =
         App::makeMetaConfig(
            meta, PLUGIN_CL_CFG_ID, "command line", "options")
            [ConfigManager::MERGE][PLUGIN_NAME];
      c["tests"][PLUGIN_CL_CFG_ID]->setType(Array);
      c["modules"][PLUGIN_CL_CFG_ID]->setType(Array);
   }

   return rval;
}

DynamicObject Tester::getCommandLineSpecs()
{
   DynamicObject spec;
   spec["help"] =
"Test options:\n"
"  -l, --level LEVEL   Adjust test output level to LEVEL. (default: 3)\n"
"                         0: No output.\n"
"                         1: Final results.\n"
"                         2: Progress (.=success, W=warning, F=failure).\n"
"                         3: Test names and PASS/WARNING/FAIL status.\n"
"                         4: Same as 3, plus test time.\n"
"                      All levels have exit status of 0 on success.\n"
"  -c                  Continue after failure. (default: true).\n"
"  -t, --test TEST     Add TEST to list of enabled tests. (default: unit).\n"
"      --test-module MODULE\n"
"                      Add MODULE to list of enabled test modules.\n"
"                      (default: all testable modules).\n"
"\n";

   DynamicObject opt;
   Config& cfg = getApp()->getMetaConfig()
      ["options"][PLUGIN_CL_CFG_ID][ConfigManager::MERGE][PLUGIN_NAME];

   opt = spec["options"]->append();
   opt["short"] = "-l";
   opt["long"] = "--level";
   opt["arg"]["root"] = cfg;
   opt["arg"]["path"] = "level";

   opt = spec["options"]->append();
   opt["short"] = "-c";
   opt["setTrue"]["root"] = cfg;
   opt["setTrue"]["path"] = "continueAfterException";

   opt = spec["options"]->append();
   opt["short"] = "-t";
   opt["long"] = "--test";
   opt["append"] = cfg["tests"][PLUGIN_CL_CFG_ID];
   opt["argError"] = "No type specified.";

   opt = spec["options"]->append();
   opt["long"] = "--test-module";
   opt["append"] = cfg["modules"][PLUGIN_CL_CFG_ID];
   opt["argError"] = "No module specified.";

   DynamicObject specs;
   specs->setType(Array);
   specs->append(spec);
   return specs;
}

bool Tester::run()
{
   bool rval = true;

   Config cfg = getApp()->getConfig()[PLUGIN_NAME];
   bool cont = cfg["continueAfterException"]->getBoolean();
   uint32_t cfgLevel = cfg["level"]->getUInt32();
   TestRunner::OutputLevel level;

   switch(cfgLevel)
   {
      case 0: level = TestRunner::None; break;
      case 1: level = TestRunner::Final; break;
      case 2: level = TestRunner::Progress; break;
      case 3: level = TestRunner::Names; break;
      default: level = TestRunner::Times; break;
   }

   TestRunner tr(getApp(), mKernel, cont, level);

   tr.group(NULL);

   // Special case "unit" testing: if any tests specified then disable "unit"
   // tests unless "unit" is specifically enabled.
   bool usingCustomTests = false;
   bool hasDefaultTest = false;

   // setup enabled tests
   {
      DynamicObjectIterator i = cfg["tests"].getIterator();
      while(i->hasNext())
      {
         DynamicObjectIterator ii = i->next().getIterator();
         while(ii->hasNext())
         {
            const char* name = ii->next()->getString();
            tr.enableTest(name);
            usingCustomTests = true;
            hasDefaultTest |= (strcmp(name, TestRunner::DEFAULT) == 0);
         }
      }
   }

   // list of specified modules under test
   // if empty then test all modules
   DynamicObject customModules;
   customModules->setType(Map);

   // setup enabled modules
   {
      DynamicObjectIterator i = cfg["modules"].getIterator();
      while(i->hasNext())
      {
         DynamicObjectIterator ii = i->next().getIterator();
         while(ii->hasNext())
         {
            customModules[ii->next()->getString()] = true;
         }
      }
   }

   // disable default test if using custom tests and default not specified
   if(usingCustomTests && !hasDefaultTest)
   {
      tr.enableTest(TestRunner::DEFAULT, false);
   }

   // load all monarch.test.TestModules and run them
   {
      MicroKernel::ModuleApiList tests;
      mKernel->getModuleApisByType("monarch.test.TestModule", tests);
      for(MicroKernel::ModuleApiList::iterator i = tests.begin();
         rval && i != tests.end(); i++)
      {
         Module* m = dynamic_cast<Module*>(*i);
         Testable* f = dynamic_cast<Testable*>(*i);
         const char* name = m->getId().name;
         // only run test if no modules listed or current module listed
         if((customModules->length() == 0) ||
            (customModules->hasMember(name) &&
            customModules[name]->getBoolean()))
         {
            tr.group(name);
            rval = f->run(tr);
            tr.ungroup();
         }
      }
   }

   assertNoException();

   tr.ungroup();
   tr.done();

   return rval;
}

class TesterFactory :
   public AppPluginFactory
{
public:
   TesterFactory() :
      AppPluginFactory(PLUGIN_NAME, "1.0")
   {
      addDependency("monarch.app.Config", "1.0");
      addDependency("monarch.app.Logging", "1.0");
   }

   virtual ~TesterFactory() {}

   virtual AppPluginRef createAppPlugin()
   {
      return new Tester(mMicroKernel);
   }
};

Module* createModestModule()
{
   return new TesterFactory();
}

void freeModestModule(Module* m)
{
   delete m;
}