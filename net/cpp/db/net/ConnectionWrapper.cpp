/*
 * Copyright (c) 2007-2008 Digital Bazaar, Inc.  All rights reserved.
 */
#include "db/net/ConnectionWrapper.h"

using namespace db::net;

ConnectionWrapper::ConnectionWrapper(Connection* c, bool cleanup)
{
   setConnection(c, cleanup);
}

ConnectionWrapper::~ConnectionWrapper()
{
   // cleanup wrapped connection as appropriate
   if(mConnection != NULL && mCleanupConnection)
   {
      delete mConnection;
   }
}

void ConnectionWrapper::setConnection(Connection* c, bool cleanup)
{
   mConnection = c;
   mCleanupConnection = cleanup;
}

inline Connection* ConnectionWrapper::getConnection()
{
   return mConnection;
}

inline bool ConnectionWrapper::mustCleanupConnection()
{
   return mCleanupConnection;
}

inline void ConnectionWrapper::setBandwidthThrottler(
   BandwidthThrottler* bt, bool read)
{
   mConnection->setBandwidthThrottler(bt, read);
}

inline BandwidthThrottler* ConnectionWrapper::getBandwidthThrottler(bool read)
{
   return mConnection->getBandwidthThrottler(read);
}

inline ConnectionInputStream* ConnectionWrapper::getInputStream()
{
   return mConnection->getInputStream();
}

inline ConnectionOutputStream* ConnectionWrapper::getOutputStream()
{
   return mConnection->getOutputStream();
}

inline uint64_t ConnectionWrapper::getBytesRead()
{
   return mConnection->getBytesRead();
}

inline uint64_t ConnectionWrapper::getBytesWritten()
{
   return mConnection->getBytesWritten();
}

inline void ConnectionWrapper::setReadTimeout(uint32_t timeout)
{
   mConnection->setReadTimeout(timeout);
}

inline void ConnectionWrapper::setWriteTimeout(uint32_t timeout)
{
   mConnection->setWriteTimeout(timeout);
}

inline void ConnectionWrapper::setSecure(bool secure)
{
   mConnection->setSecure(secure);
}

inline bool ConnectionWrapper::isSecure()
{
   return mConnection->isSecure();
}

inline bool ConnectionWrapper::isClosed()
{
   return mConnection->isClosed();
}

inline void ConnectionWrapper::close()
{
   mConnection->close();
}

inline bool ConnectionWrapper::getLocalAddress(InternetAddress* address)
{
   return mConnection->getLocalAddress(address);
}

inline bool ConnectionWrapper::getRemoteAddress(InternetAddress* address)
{
   return mConnection->getRemoteAddress(address);
}

inline void ConnectionWrapper::setSocket(Socket* s, bool cleanup)
{
   mConnection->setSocket(s, cleanup);
}

inline Socket* ConnectionWrapper::getSocket()
{
   return mConnection->getSocket();
}

inline bool ConnectionWrapper::mustCleanupSocket()
{
   return mConnection->mustCleanupSocket();
}
