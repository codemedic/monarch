/*
 * Copyright (c) 2007 Digital Bazaar, Inc.  All rights reserved.
 */
#include "HttpTrailer.h"

using namespace db::net::http;

HttpTrailer::HttpTrailer()
{
}

HttpTrailer::~HttpTrailer()
{
}

void HttpTrailer::update(unsigned long long contentLength)
{
   // set content-length header to ensure it is accurate
   setField("Content-Length", contentLength);
}
