/*
 * Copyright (c) 2007-2010 Digital Bazaar, Inc. All rights reserved.
 */
#include "monarch/net/InternetAddress.h"

#include "monarch/net/WindowsSupport.h"
#include "monarch/rt/DynamicObject.h"
#include "monarch/rt/Exception.h"
#include "monarch/net/SocketTools.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace std;
using namespace monarch::net;
using namespace monarch::rt;

InternetAddress::InternetAddress(const char* host, unsigned short port) :
   SocketAddress(SocketAddress::IPv4, "0.0.0.0", port)
{
   mHost = strdup("");

   if(strcmp(host, "") != 0)
   {
      // resolve host
      InternetAddress::setHost(host);
   }
}

InternetAddress::~InternetAddress()
{
   free(mHost);
}

bool InternetAddress::toSockAddr(sockaddr* addr, unsigned int& size)
{
   bool rval = false;

   // use sockaddr_in (IPv4)
   if(size >= sizeof(sockaddr_in))
   {
      struct sockaddr_in* sa = (sockaddr_in*)addr;
      size = sizeof(sockaddr_in);
      memset(sa, '\0', size);

      // the address family is internet (AF_INET = address family internet)
      sa->sin_family = AF_INET;

      // htons = "Host To Network Short" which means order the short in
      // network byte order (big-endian)
      sa->sin_port = htons(getPort());

      // converts an address to network byte order
      rval = (inet_pton(AF_INET, getAddress(), &sa->sin_addr) == 1);
   }

   return rval;
}

bool InternetAddress::fromSockAddr(const sockaddr* addr, unsigned int size)
{
   bool rval = false;

   // use sockaddr_in (IPv4)
   if(size >= sizeof(sockaddr_in))
   {
      struct sockaddr_in* sa = (sockaddr_in*)addr;

      // get address
      char dst[INET_ADDRSTRLEN];
      memset(&dst, '\0', size);
      if(inet_ntop(AF_INET, &sa->sin_addr, dst, INET_ADDRSTRLEN) != NULL)
      {
         // set address
         setAddress(dst);

         // converting from network byte order to little-endian
         setPort(ntohs(sa->sin_port));

         // conversion successful
         rval = true;
      }
   }

   return rval;
}

void InternetAddress::setAddress(const char* address)
{
   // set the address
   SocketAddress::setAddress(address);

   // clear the host
   free(mHost);
   mHost = strdup("");
}

bool InternetAddress::setHost(const char* host)
{
   bool rval = false;

   // create hints address structure
   struct addrinfo hints;
   memset(&hints, '\0', sizeof(hints));
   hints.ai_family = AF_INET;

   // create pointer for storing allocated resolved address
   struct addrinfo* res = NULL;

   // get address information
   if(getaddrinfo(host, NULL, &hints, &res) != 0)
   {
      ExceptionRef e = new Exception(
         "Unknown host.",
         "monarch.net.UnknownHost");
      e->getDetails()["host"] = host;
      Exception::set(e);
   }
   else
   {
      // copy the first result
      struct sockaddr_in addr;
      memcpy(&addr, res->ai_addr, res->ai_addrlen);

      // get the address
      char dst[INET_ADDRSTRLEN];
      memset(&dst, '\0', INET_ADDRSTRLEN);
      inet_ntop(AF_INET, &addr.sin_addr, dst, INET_ADDRSTRLEN);
      free(mAddress);
      mAddress = strdup(dst);
      rval = true;

      // save the host
      free(mHost);
      mHost = strdup(host);
   }

   if(res != NULL)
   {
      // free res if it got allocated
      freeaddrinfo(res);
   }

   return rval;
}

const char* InternetAddress::getHost()
{
   if(strcmp(mHost, "") == 0 && strcmp(getAddress(), "") != 0)
   {
      // get a IPv4 address structure
      struct sockaddr_in sa;
      unsigned int size = sizeof(sockaddr_in);
      toSockAddr((sockaddr*)&sa, size);

      // NULL specifies that we don't care about getting a "service" name
      // given in sockaddr_in will be returned
      char dst[100];
      memset(&dst, '\0', 100);
      if(getnameinfo((sockaddr*)&sa, size, dst, 100, NULL, 0, 0) == 0)
      {
         // set host name
         free(mHost);
         mHost = strdup(dst);
      }
      else
      {
         // use address
         free(mHost);
         mHost = strdup(getAddress());
      }
   }

   // return host
   return mHost;
}

bool InternetAddress::isMulticast()
{
   bool rval = false;

   // get a IPv4 address structure
   struct sockaddr_in sa;
   unsigned int size = sizeof(sockaddr_in);
   toSockAddr((sockaddr*)&sa, size);

   if(IN_MULTICAST(ntohl(sa.sin_addr.s_addr)) != 0)
   {
      rval = true;
   }

   return rval;
}

string InternetAddress::toString(bool simple, bool port)
{
   string rval;

   if(simple)
   {
      if(port)
      {
         char temp[6 + strlen(getAddress())];
         sprintf(temp, "%s:%u", getAddress(), getPort());
         rval = temp;
      }
      else
      {
         rval = getAddress();
      }
   }
   else
   {
      string host;
      if(strcmp(getAddress(), "0.0.0.0") == 0)
      {
         // use local hostname
         host = SocketTools::getHostname();
      }
      else
      {
         host = getHost();
      }

      int length = 100 + host.length() + strlen(getAddress());
      char temp[length];
      if(port)
      {
         snprintf(temp, length, "InternetAddress [%s:%u,%s:%u]",
            host.c_str(), getPort(), getAddress(), getPort());
         rval = temp;
      }
      else
      {
         snprintf(temp, length, "InternetAddress [%s,%s]",
            host.c_str(), getAddress());
         rval = temp;
      }
   }

   return rval;
}
