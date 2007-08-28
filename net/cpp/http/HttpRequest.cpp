/*
 * Copyright (c) 2007 Digital Bazaar, Inc.  All rights reserved.
 */
#include "HttpRequest.h"
#include "HttpConnection.h"
#include "HttpResponse.h"

using namespace db::io;
using namespace db::net;
using namespace db::net::http;

HttpRequest::HttpRequest(HttpConnection* hc) : WebRequest(hc)
{
}

HttpRequest::~HttpRequest()
{
}

WebResponse* HttpRequest::createResponse()
{
   return new HttpResponse(this);
}

IOException* HttpRequest::sendHeader()
{
   return getConnection()->sendHeader(getHeader());
}

IOException* HttpRequest::receiveHeader()
{
   return getConnection()->receiveHeader(getHeader());
}

IOException* HttpRequest::sendBody(InputStream* is, HttpHeader* trailers)
{
   return getConnection()->sendBody(getHeader(), is, trailers);
}

IOException* HttpRequest::receiveBody(OutputStream* os, HttpHeader* trailers)
{
   return getConnection()->receiveBody(getHeader(), os, trailers);
}

HttpRequestHeader* HttpRequest::getHeader()
{
   return &mHeader;
}

HttpConnection* HttpRequest::getConnection()
{
   return (HttpConnection*)mConnection;
}
