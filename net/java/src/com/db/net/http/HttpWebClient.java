/*
 * Copyright (c) 2005-2006 Digital Bazaar, Inc.  All rights reserved.
 */
package com.db.net.http;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.InetSocketAddress;
import java.net.MalformedURLException;
import java.net.Socket;
import java.net.URL;
import java.security.KeyStore;

import javax.net.ssl.KeyManager;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;

import com.db.logging.Logger;
import com.db.logging.LoggerManager;
import com.db.net.WebConnection;
import com.db.net.WebConnectionClient;
import com.db.net.ssl.TrustAllSSLManager;

/**
 * This client is used to connect to a web server that supports
 * http (HyperText Transfer Protocol).
 * 
 * @author Dave Longley
 */
public class HttpWebClient implements WebConnectionClient
{
   /**
    * The URL to connect to.
    */
   protected URL mUrl;
   
   /**
    * The connection timeout to use (in milliseconds).
    */
   protected int mConnectionTimeout;

   /**
    * The SSL socket factory for creating SSL sockets.
    */
   protected SSLSocketFactory mSSLSocketFactory;
   
   /**
    * The default user-agent name for this client.
    */
   public static final String DEFAULT_USER_AGENT =
      "Digital Bazaar Http Client 1.0";
   
   /**
    * Creates a new http web client with no specified URL to connect to.
    */
   public HttpWebClient()   
   {
      this((URL)null);
   }
   
   /**
    * Creates a new HttpWebClient with the given host and port.
    * 
    * @param host the host to connect to.
    * @param port the port to connect on.
    * 
    * @throws MalformedURLException
    */
   public HttpWebClient(String host, int port) throws MalformedURLException
   {
      this(host + ":" + port);
   }

   /**
    * Creates a new HttpWebClient with the given URL.
    * 
    * @param url the URL to connect to.
    * 
    * @throws MalformedURLException
    */
   public HttpWebClient(String url) throws MalformedURLException
   {
      this(new URL(url));
   }
   
   /**
    * Creates a new HttpWebClient with the given URL.
    * 
    * @param url the URL to connect to.
    */
   public HttpWebClient(URL url)
   {
      // set the URL
      setUrl(url);
      
      // set the default connection timeout to 10 seconds
      setConnectionTimeout(10000);
   }
   
   /**
    * Creates the internal SSL socket factory.
    * 
    * @param keyManagers the key managers for the factory.
    * @param trustManagers the trust managers for the factory.
    */
   protected void createSSLSocketFactory(
      KeyManager[] keyManagers, TrustManager[] trustManagers)
   {
      try
      {
         // create ssl context
         SSLContext sslContext = SSLContext.getInstance("TLS");

         // initialize ssl context
         sslContext.init(keyManagers, trustManagers, null);
         
         // create the ssl factory
         mSSLSocketFactory = sslContext.getSocketFactory();
      }
      catch(Throwable t)
      {
         getLogger().debug(getClass(), 
            "could not create SSL socket factory.");
         getLogger().debug(getClass(), LoggerManager.getStackTrace(t));
         
         // set ssl factory to null
         mSSLSocketFactory = null;
      }
   }
   
   /**
    * Gets the SSL socket factory.
    * 
    * @return the SSL socket factory.
    */
   protected SSLSocketFactory getSSLSocketFactory()
   {
      if(mSSLSocketFactory == null)
      {
         // create default SSL socket factory
         
         // use trust all manager
         TrustManager[] trustManagers =
            new TrustManager[]{new TrustAllSSLManager()};
         
         // create SSLSocketFactory
         createSSLSocketFactory(null, trustManagers);
      }
      
      return mSSLSocketFactory;
   }
   
   /**
    * Gets an http web connection to the specified URL.
    * 
    * @param url the URL to connect to.
    * 
    * @return the http web connection or null if the connection could
    *         not be made.
    */
   protected HttpWebConnection getWebConnection(URL url)
   {
      HttpWebConnection hwc = null;
      
      try
      {
         // create an unconnected socket
         Socket socket = new Socket();
         
         // get the port to connect to
         int port = url.getPort();
         if(port == -1)
         {
            // get the default port
            port = url.getDefaultPort();
         }
         
         // connect the socket with a timeout of 10 seconds
         InetSocketAddress address = new InetSocketAddress(url.getHost(), port);
         socket.connect(address, getConnectionTimeout());
         
         if(url.getProtocol().equals("https"))
         {
            // wrap the socket with an SSL socket
            socket = getSSLSocketFactory().createSocket(
               socket, url.getHost(), port, true);
            
            // set the enabled cipher suites
            String[] suites = getSSLSocketFactory().getSupportedCipherSuites();
            ((SSLSocket)socket).setEnabledCipherSuites(suites);
         }
         
         // create web connection
         hwc = new HttpWebConnection(socket);
         
         getLogger().debug(getClass(), "connected to: " + url.toString());
      }
      catch(Throwable t)
      {
         getLogger().debug(getClass(), 
            "could not establish an http web connection" +
            ",url='" + url.toString() + "'" +
            ",reason=" + t);
         getLogger().debug(getClass(), LoggerManager.getStackTrace(t));
         
         if(hwc != null)
         {
            hwc.disconnect();
         }
         
         hwc = null;
      }
      
      return hwc;
   }
   
   /**
    * Attempts to connect to the stored endpoint address.
    * 
    * If a connection cannot be established, the connection will be retried
    * multiple times.
    * 
    * @return the HttpWebConnection to the endpoint address or null if failure.
    */
   public synchronized HttpWebConnection connect()
   {
      return connect(2);
   }
   
   /**
    * Attempts to connect to the stored endpoint address.
    * 
    * If a connection cannot be established, the connection can be retried
    * multiple times.
    * 
    * @param retries the number of connection retries to attempt.
    * 
    * @return the HttpWebConnection to the endpoint address or null if failure.
    */
   public synchronized HttpWebConnection connect(int retries)
   {
      HttpWebConnection rval = null;
      
      rval = connect(getUrl(), retries);
      
      return rval;
   }
   
   /**
    * Attempts to connect to the passed url. If a connection cannot be
    * established, the connection will be retried multiple times.
    * 
    * @param url the url to connect to.
    * 
    * @return the web connection to the url or null if failure.
    */
   public synchronized WebConnection connect(String url)   
   {
      return connect(url, 2);
   }
   
   /**
    * Attempts to connect to the passed url. If a connection cannot be
    * established, the connection can be retried multiple times.
    * 
    * @param url the url to connect to.
    * @param retries the number of connection retries to attempt.
    * 
    * @return the web connection to the url or null if failure.
    */
   public synchronized WebConnection connect(String url, int retries)
   {
      HttpWebConnection wc = null;
      
      try
      {
         wc = connect(new URL(url), retries);
      }
      catch(MalformedURLException e)
      {
         getLogger().error(getClass(), 
            "could not establish an http web connection! " +
            "URL was malformed!,url=" + url);
      }
      
      return wc;
   }
   
   /**
    * Attempts to connect to the passed URL. If a connection cannot be
    * established, the connection will be retried multiple times.
    * 
    * @param url the URL to connect to.
    * 
    * @return the web connection to the URL or null if failure.
    */
   public synchronized HttpWebConnection connect(URL url)
   {
      return connect(url, 2);
   }
   
   /**
    * Attempts to connect to the passed URL. If a connection cannot be
    * established, the connection can be retried multiple times.
    * 
    * @param url the URL to connect to.
    * @param retries the number of connection retries to attempt.
    * 
    * @return the web connection to the URL or null if failure.
    */
   public synchronized HttpWebConnection connect(URL url, int retries)
   {
      HttpWebConnection wc = null;
      
      if(url != null)
      {
         getLogger().debug(getClass(), 
            "trying to establish an http web connection to '" +
            url + "'...");
            
         // try to get a web connection
         wc = getWebConnection(url);
         
         // try twice more if a web connection could not be established
         for(int i = 0; i < retries && wc == null &&
             !Thread.currentThread().isInterrupted(); i++)
         {
            wc = getWebConnection(url);
         }
            
         if(wc != null)
         {
            getLogger().debug(getClass(),
               "http web connection established," +
               "ip=" + wc.getRemoteHostName() + ":" + wc.getRemotePort());
         }
         else
         {
            getLogger().error(getClass(), 
               "could not establish an http web connection!,url=" + url);
         }
      }
      else
      {
         getLogger().error(getClass(), 
            "could not establish an http web connection! " +
            "URL was null or blank!");
      }
      
      return wc;
   }
   
   /**
    * Sends an http web request from the client to the server.
    * 
    * @param request the http web request to send to the server.
    * @param is the input stream to read the request body from.
    * 
    * @return true if the request was successfully sent, false if not.
    */
   public boolean sendRequest(HttpWebRequest request, InputStream is)
   {
      boolean rval = false;
      
      if(request.sendHeader())
      {
         rval = request.sendBody(is);
      }
      
      return rval;
   }
   
   /**
    * Sends an http web request from the client to the server.
    * 
    * @param request the http web request to send to the server.
    * @param body the body to send along with the request.
    * 
    * @return true if the request was successfully sent, false if not.
    */
   public boolean sendRequest(HttpWebRequest request, String body)
   {
      boolean rval = false;
      
      if(request.sendHeader())
      {
         rval = request.sendBody(body);
      }
      
      return rval;
   }
   
   /**
    * Sends an http web request from the client to the server.
    * 
    * @param request the http web request to send to the server.
    * @param body the body to send along with the request.
    * 
    * @return true if the request was successfully sent, false if not.
    */
   public boolean sendRequest(HttpWebRequest request, byte[] body)
   {
      boolean rval = false;
      
      if(request.sendHeader())
      {
         rval = request.sendBody(body);
      }
      
      return rval;
   }   
   
   /**
    * Sends an http web request from the client to the server.
    * 
    * @param request the http web request to send to the server.
    * 
    * @return true if the request was successfully sent, false if not.
    */
   public boolean sendRequest(HttpWebRequest request)
   {
      boolean rval = false;
      
      rval = request.sendHeader();
      
      return rval;
   }

   /**
    * Recieves the response header from the server.
    * 
    * @param response the http response to read with.
    * 
    * @return true if the response header could be read, false if not.
    */
   public boolean receiveResponseHeader(HttpWebResponse response)
   {
      boolean rval = false;
      
      // keep reading while HTTP 1xx
      boolean received = false;
      while((received = response.receiveHeader()) &&
            response.getHeader().getStatusCode().startsWith("1"))
      {
         // read and discard body if status code is 1xx
         response.receiveBody();
      }
      
      // set whether or not the header was read
      if(received)
      {
         rval = !response.getHeader().getStatusCode().startsWith("1");
      }
      
      return rval;
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve content
    * as a string. The GET will be retried multiple times upon failure.
    * 
    * @param url the url to connect to for the content.
    * @param path the relative path to the content.
    * 
    * @return the retrieved content or null if no content could be retrieved.
    * 
    * @throws MalformedURLException
    */
   public String getContent(String url, String path)
   throws MalformedURLException
   {
      return getContent(url, path, 2);
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve content
    * as a string. The GET can be retried multiple times upon failure.
    * 
    * @param url the url to connect to for the content.
    * @param path the relative path to the content.
    * @param retries the number of connection retries to attempt.
    * 
    * @return the retrieved content or null if no content could be retrieved.
    * 
    * @throws MalformedURLException
    */
   public String getContent(String url, String path, int retries)
   throws MalformedURLException
   {
      return getContent(new URL(url), path, retries);
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve content
    * as a string. The GET will be retried multiple times upon failure.
    * 
    * @param url the url to connect to for the content.
    * @param path the relative path to the content.
    * 
    * @return the retrieved content or null if no content could be retrieved.
    */
   public String getContent(URL url, String path)
   {
      return getContent(url, path, 2);
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve content
    * as a string. The GET can be retried multiple times upon failure.
    * 
    * @param url the url to connect to for the content.
    * @param path the relative path to the content.
    * @param retries the number of connection retries to attempt.
    * 
    * @return the retrieved content or null if no content could be retrieved.
    */
   public String getContent(URL url, String path, int retries)
   {
      String rval = null;
      
      // get a web connection
      HttpWebConnection connection = connect(url, retries);
      if(connection != null)
      {
         // create http web request
         HttpWebRequest request = new HttpWebRequest(connection);
         request.getHeader().setMethod("GET");
         request.getHeader().setPath(path);
         request.getHeader().setVersion("HTTP/1.1");
         request.getHeader().setHost(connection.getRemoteHost());
         request.getHeader().setUserAgent(DEFAULT_USER_AGENT);
         request.getHeader().setConnection("close");
         
         // send request
         if(sendRequest(request))
         {
            // receive response header
            HttpWebResponse response = request.createHttpWebResponse();
            if(receiveResponseHeader(response))
            {
               // see if response was OK
               if(response.getHeader().hasOKStatusCode())
               {
                  // receive body string
                  rval = response.receiveBodyString();
               }
            }
         }
         
         // disconnect
         connection.disconnect();
      }
      
      return rval;      
   }
   
   /**
    * A convenience method for performing an HTTP POST to send content
    * (application/x-www-form-urlencoded) as a string and receive it as a
    * string.
    * 
    * @param url the url to POST to.
    * @param content the content to POST.
    * 
    * @return the retrieved content or null if no content was retrieved.
    * 
    * @exception IOException thrown if an IO error occurs.
    */
   public String post(URL url, String content) throws IOException
   {
      return post(url, content, 0);
   }
   
   /**
    * A convenience method for performing an HTTP POST to send content
    * (application/x-www-form-urlencoded) as a string and receive it as a
    * string. The POST can be retried multiple times upon failure.
    * 
    * @param url the url to POST to.
    * @param content the content to POST.
    * @param retries the number of connection retries to attempt.
    * 
    * @return the retrieved content or null if no content was retrieved.
    * 
    * @exception IOException thrown if an IO error occurs.
    */
   public String post(URL url, String content, int retries)
   throws IOException
   {
      String rval = null;
      
      // get a web connection
      HttpWebConnection connection = connect(url, retries);
      if(connection != null)
      {
         // create http web request
         HttpWebRequest request = new HttpWebRequest(connection);
         request.getHeader().setMethod("POST");
         request.getHeader().setPath(url.getPath());
         request.getHeader().setVersion("HTTP/1.1");
         request.getHeader().setContentType(
            "application/x-www-form-urlencoded");
         request.getHeader().setContentLength(content.length());
         request.getHeader().setHost(connection.getRemoteHost());
         request.getHeader().setUserAgent(DEFAULT_USER_AGENT);
         request.getHeader().setConnection("close");
         
         // send request
         if(sendRequest(request, content))
         {
            // receive response header
            HttpWebResponse response = request.createHttpWebResponse();
            if(receiveResponseHeader(response))
            {
               // see if response was OK
               if(response.getHeader().hasOKStatusCode())
               {
                  // receive body string
                  rval = response.receiveBodyString();
               }
            }
            else
            {
               throw new IOException(
                  "Could not receive response from " + url.toString());
            }
         }
         else
         {
            throw new IOException(
               "Could not send request to " + url.toString());
         }
         
         // disconnect
         connection.disconnect();
      }
      else
      {
         throw new IOException("Could not connect to " + url.toString());
      }
      
      return rval;
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve a file. The
    * GET will be performed multiple times upon failure.
    * 
    * @param url the url to connect to for the file.
    * @param path the relative path to the file.
    * @param directory the directory to store the file in.
    * 
    * @return the file if it was received or null if the file could not
    *         be received.
    *         
    * @throws MalformedURLException
    */
   public File getFile(String url, String path, File directory)
   throws MalformedURLException
   {
      return getFile(url, path, directory, 2);
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve a file. The
    * GET can be performed multiple times upon failure.
    * 
    * @param url the url to connect to for the file.
    * @param path the relative path to the file.
    * @param directory the directory to store the file in.
    * @param retries the number of connection retries to attempt.
    * 
    * @return the file if it was received or null if the file could not
    *         be received.
    *         
    * @throws MalformedURLException
    */
   public File getFile(String url, String path, File directory, int retries)
   throws MalformedURLException
   {
      return getFile(new URL(url), path, directory, retries);
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve a file. The
    * GET will be performed multiple times upon failure.
    * 
    * @param url the URL to connect to for the file.
    * @param path the relative path to the file.
    * @param directory the directory to store the file in.
    * 
    * @return the file if it was received or null if the file could not
    *         be received.
    */
   public File getFile(URL url, String path, File directory)
   {
      return getFile(url, path, directory, 2);
   }
   
   /**
    * A convenience method for performing an HTTP GET to retrieve a file. The
    * GET can be performed multiple times upon failure.
    * 
    * @param url the URL to connect to for the file.
    * @param path the relative path to the file.
    * @param directory the directory to store the file in.
    * @param retries the number of connection retries to attempt.
    * 
    * @return the file if it was received or null if the file could not
    *         be received.
    */
   public File getFile(URL url, String path, File directory, int retries)
   {
      File rval = null;
      
      // get a web connection
      HttpWebConnection connection = connect(url, retries);
      if(connection != null)
      {
         // create http web request
         HttpWebRequest request = new HttpWebRequest(connection);
         request.getHeader().setMethod("GET");
         request.getHeader().setPath(path);
         request.getHeader().setVersion("HTTP/1.1");
         request.getHeader().setHost(connection.getRemoteHost());
         request.getHeader().setUserAgent(DEFAULT_USER_AGENT);
         request.getHeader().setConnection("close");
         
         // send request
         if(sendRequest(request))
         {
            // receive response header
            HttpWebResponse response = request.createHttpWebResponse();
            if(receiveResponseHeader(response))
            {
               // see if response was OK
               if(response.getHeader().hasOKStatusCode())
               {
                  // get the file name
                  String filename = response.getHeader().
                     getContentDispositionValue("filename");
                  
                  if(filename == null)
                  {
                     try
                     {
                        File file = File.createTempFile(
                           "bmdownload", ".tmp", directory);
                        file.delete();
                        filename = file.getName();
                     }
                     catch(IOException e)
                     {
                        filename = "tempfile.tmp";
                     }
                  }
                  
                  // get full path of file to write to
                  String fullPath =
                     directory.getAbsolutePath() + File.separator + filename;
                  
                  // create file output stream reference
                  FileOutputStream fos = null;
                  
                  try
                  {
                     // create file output stream for writing to the file
                     fos = new FileOutputStream(fullPath);

                     // receive response body
                     response.receiveBody(fos);
                     
                     // close the file output stream
                     fos.close();
                     
                     // file received
                     rval = new File(fullPath);
                  }
                  catch(IOException e)
                  {
                     getLogger().error(getClass(),
                        "An exception occurred while receiving a file!," +
                        "exception= e");
                     getLogger().debug(getClass(), Logger.getStackTrace(e));
                  }
                  
                  try
                  {
                     // ensure file output stream is closed
                     if(fos != null)
                     {
                        fos.close();
                     }
                  }
                  catch(IOException ignore)
                  {
                  }
               }
            }
         }
         
         // disconnect
         connection.disconnect();
      }
      
      return rval;
   }
   
   /**
    * Sets an ssl certificate (from a keystore) to trust. Uses
    * the default algorithm for the certificate (SunX509).
    * 
    * @param keystore the name of the keystore file.
    * @param password the password to unlock the keystore.
    * 
    * @return true if the ssl certificate was successfully loaded,
    *         false if not.
    */
   public boolean setTrustedSSLKeystore(String keystore, String password)
   {
      return setTrustedSSLKeystore(keystore, password, "SunX509");
   }

   /**
    * Sets an ssl certificate (from a keystore) to trust.
    * 
    * @param keystore the name of the keystore file.
    * @param password the password to unlock the keystore.
    * @param algorithm the algorithm for the keystore.
    * 
    * @return true if the ssl certificate was successfully loaded,
    *         false if not.
    */
   public synchronized boolean setTrustedSSLKeystore(
      String keystore, String password, String algorithm)
   {
      boolean rval = false;
      
      try
      {
         // load keystore
         KeyStore ks = KeyStore.getInstance("JKS");
         ks.load(new FileInputStream(keystore), password.toCharArray());
         
         // get key managers
         KeyManagerFactory kmf = KeyManagerFactory.getInstance(algorithm);
         kmf.init(ks, password.toCharArray());
         
         // get trust managers
         String alg = TrustManagerFactory.getDefaultAlgorithm();
         TrustManagerFactory tmf = TrustManagerFactory.getInstance(alg);
         tmf.init(ks);
         
         // create SSL socket factory
         createSSLSocketFactory(kmf.getKeyManagers(), tmf.getTrustManagers());

         rval = true;
      }
      catch(Throwable t)
      {
         getLogger().error(getClass(), 
            "Could not load keystore!, keystore=" + keystore);
         getLogger().debug(getClass(), Logger.getStackTrace(t));
      }
      
      return rval;
   }   
   
   /**
    * Gets the path to the web service.
    * 
    * @return the path to the web service.
    */
   public synchronized String getWebServicePath()
   {
      String rval = null;
      
      if(getUrl() != null)
      {
         // get the URL path
         rval = getUrl().getPath();
      }
      
      return rval;
   }
   
   /**
    * Sets the URL to connect to.
    * 
    * @param url the URL to connect to.
    * 
    * @throws MalformedURLException
    */
   public void setUrl(String url) throws MalformedURLException
   {
      setUrl(new URL(url));
   }
   
   /**
    * Sets the URL to connect to.
    * 
    * @param url the URL to connect to.
    */
   public synchronized void setUrl(URL url)
   {
      mUrl = url;
   }
   
   /**
    * Gets the URL to connect to.
    *
    * @return the URl to connect to.
    */
   public synchronized URL getUrl()
   {
      return mUrl;
   }
   
   /**
    * Sets the connection timeout for this client to use when connecting.
    * 
    * @param timeout the connection timeout (in milliseconds) for this client.
    */
   public synchronized void setConnectionTimeout(int timeout)
   {
      mConnectionTimeout = timeout;
   }
   
   /**
    * Gets the connection timeout for this client to use when connecting.
    * 
    * @return the connection timeout (in milliseconds) for this client.
    */
   public synchronized int getConnectionTimeout()
   {
      return mConnectionTimeout;
   }
   
   /**
    * Gets the logger for this http web client.
    * 
    * @return the logger for this http web client.
    */
   public Logger getLogger()
   {
      return LoggerManager.getLogger("dbnet");
   }
}
