/*
 * Copyright (c) 2007-2008 Digital Bazaar, Inc.  All rights reserved.
 */
#include <iostream>
#include <sstream>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/engine.h>

#include "db/test/Test.h"
#include "db/test/Tester.h"
#include "db/test/TestRunner.h"
#include "db/rt/Thread.h"
#include "db/crypto/AsymmetricKeyFactory.h"
#include "db/crypto/BigDecimal.h"
#include "db/crypto/DigitalEnvelope.h"
#include "db/crypto/DigitalSignatureInputStream.h"
#include "db/crypto/DigitalSignatureOutputStream.h"
#include "db/crypto/DefaultBlockCipher.h"
#include "db/crypto/MessageDigest.h"
#include "db/io/File.h"
#include "db/io/FileInputStream.h"
#include "db/io/FileOutputStream.h"
#include "db/io/FileList.h"
#include "db/io/ByteArrayInputStream.h"
#include "db/io/OStreamOutputStream.h"

using namespace std;
using namespace db::test;
using namespace db::crypto;
using namespace db::io;
using namespace db::rt;
using namespace db::util;

void runMessageDigestTest(TestRunner& tr)
{
   tr.test("MessageDigest");
   
   // correct values
   string correctMd5 = "78eebfd9d42958e3f31244f116ab7bbe";
   string correctSha1 = "5f24f4d6499fd2d44df6c6e94be8b14a796c071d";   
   
   MessageDigest testMd5("MD5");
   testMd5.update("THIS ");
   testMd5.update("IS A");
   testMd5.update(" MESSAGE");
   string digestMd5 = testMd5.getDigest();
   
   //cout << "MD5 Digest=" << digestMd5 << endl;
   assert(digestMd5 == correctMd5);
   
   MessageDigest testSha1("SHA1");
   testSha1.update("THIS IS A MESSAGE");
   string digestSha1 = testSha1.getDigest();
   
   //cout << "SHA-1 Digest=" << digestSha1 << endl;
   assert(digestSha1 == correctSha1);
   
   tr.pass();
}


void runCipherTest(TestRunner& tr, const char* algorithm)
{
   tr.group("Cipher");
   
   // include crypto error strings
   ERR_load_crypto_strings();
   
   // add all algorithms
   OpenSSL_add_all_algorithms();
   
   // seed PRNG
   //RAND_load_file("/dev/urandom", 1024);
   
   tr.test(algorithm);
   {
      // create a secret message
      char message[] = "I'll never teelllll!";
      int length = strlen(message);
      
      // get a default block cipher
      DefaultBlockCipher cipher;
      
      // generate a new key and start encryption
      SymmetricKey* key = NULL;
      cipher.startEncrypting(algorithm, &key);
      assert(key != NULL);
      
      // update encryption
      char output[2048];
      int outLength;
      int totalOut = 0;
      cipher.update(message, length, output, outLength);
      totalOut += outLength;
      
      // finish encryption
      cipher.finish(output + outLength, outLength);
      totalOut += outLength;
      
      // start decryption
      cipher.startDecrypting(key);
      
      // update decryption
      char input[2048];
      int inLength;
      int totalIn = 0;
      cipher.update(output, totalOut, input, inLength);
      totalIn += inLength;
      
      // finish decryption
      cipher.finish(input + inLength, inLength);
      totalIn += inLength;
      
      // cleanup key
      delete key;
      
      // check the decrypted message
      string result(input, totalIn);
      assert(strcmp(message, result.c_str()) == 0);
   }
   tr.passIfNoException();
   
   // do byte buffer test
   string alg = algorithm;
   alg.append("+ByteBuffer");
   tr.test(alg.c_str());
   {
      // create a secret message
      char message[] = "I'll never teelllll!";
      int length = strlen(message);
      
      // get a default block cipher
      DefaultBlockCipher cipher;
      
      // generate a new key and start encryption
      SymmetricKey* key = NULL;
      cipher.startEncrypting(algorithm, &key);
      assert(key != NULL);
      
      // update and finish encryption
      ByteBuffer output;
      cipher.update(message, length, &output, true);
      cipher.finish(&output, true);
      
      // do decryption
      ByteBuffer input;
      cipher.startDecrypting(key);
      cipher.update(output.data(), output.length(), &input, true);
      cipher.finish(&input, true);
      
      // cleanup key
      delete key;
      
      // check the decrypted message
      string result(input.data(), input.length());
      assert(strcmp(message, result.c_str()) == 0);
   }
   tr.passIfNoException();
   
   // clean up crypto strings
   EVP_cleanup();
   
   tr.ungroup();
}

void runAsymmetricKeyLoadingTest(TestRunner& tr)
{
   tr.test("Asymmetric Key Loading");
   
   // include crypto error strings
   ERR_load_crypto_strings();
   
   // add all algorithms
   OpenSSL_add_all_algorithms();
   
   // seed PRNG
   //RAND_load_file("/dev/urandom", 1024);
   
   // get an asymmetric key factory
   AsymmetricKeyFactory factory;
   
   // create a new key pair
   PrivateKey* privateKey;
   PublicKey* publicKey;
   factory.createKeyPair("RSA", &privateKey, &publicKey);
   
   assert(privateKey != NULL);
   assert(publicKey != NULL);
   
   // write keys to PEMs
   string privatePem = factory.writePrivateKeyToPem(privateKey, "password");
   string publicPem = factory.writePublicKeyToPem(publicKey);   
   
   // cleanup keys
   delete privateKey;
   delete publicKey;
   privateKey = NULL;
   publicKey = NULL;
   
   // load the private key from PEM
   privateKey = factory.loadPrivateKeyFromPem(
      privatePem.c_str(), privatePem.length(), "password");
   
   // load the public key from PEM
   publicKey = factory.loadPublicKeyFromPem(
      publicPem.c_str(), publicPem.length());
   
   assert(privateKey != NULL);
   assert(publicKey != NULL);
   
   // sign some data
   char data[] = {1,2,3,4,5,6,7,8};
   DigitalSignature* ds1 = privateKey->createSignature();
   ds1->update(data, 8);
   
   // get the signature
   char sig[ds1->getValueLength()];
   unsigned int length;
   ds1->getValue(sig, length);
   delete ds1;
   
   // verify the signature
   DigitalSignature* ds2 = publicKey->createSignature();
   ds2->update(data, 8);
   bool verified = ds2->verify(sig, length);
   delete ds2;
   
   assert(verified);
   
   // cleanup keys
   delete privateKey;
   delete publicKey;
   
   // clean up crypto strings
   EVP_cleanup();

   tr.passIfNoException();
}

void runDsaAsymmetricKeyCreationTest(TestRunner& tr)
{
   tr.test("DSA Asymmetric Key Creation");
   
   // include crypto error strings
   ERR_load_crypto_strings();
   
   // add all algorithms
   OpenSSL_add_all_algorithms();
   
   // seed PRNG
   //RAND_load_file("/dev/urandom", 1024);
   
   // get an asymmetric key factory
   AsymmetricKeyFactory factory;
   
   // create a new key pair
   PrivateKey* privateKey;
   PublicKey* publicKey;
   factory.createKeyPair("DSA", &privateKey, &publicKey);
   
   assert(privateKey != NULL);
   assert(publicKey != NULL);
   
   // test copy constructors
   PrivateKey prvKey(*privateKey);
   PublicKey pubKey(*publicKey);
   
   // cleanup private key
   if(privateKey != NULL)
   {
      delete privateKey;
   }
   
   // cleanup public key
   if(publicKey != NULL)
   {
      delete publicKey;
   }
   
   privateKey = &prvKey;
   publicKey = &pubKey;
   
   assertStrCmp(privateKey->getAlgorithm(), "DSA");
   assertStrCmp(publicKey->getAlgorithm(), "DSA");
   
   // sign some data
   char data[] = {1,2,3,4,5,6,7,8};
   DigitalSignature* ds1 = privateKey->createSignature();
   ds1->update(data, 8);
   
   // get the signature
   char sig[ds1->getValueLength()];
   unsigned int length;
   ds1->getValue(sig, length);
   delete ds1;
   
   // verify the signature
   DigitalSignature* ds2 = publicKey->createSignature();
   ds2->update(data, 8);
   bool verified = ds2->verify(sig, length);
   delete ds2;
   
   assert(verified);
   if(verified)
   
   string outPrivatePem =
      factory.writePrivateKeyToPem(privateKey, "password");
   string outPublicPem =
      factory.writePublicKeyToPem(publicKey);
   
   //cout << "Written Private Key PEM=" << endl << outPrivatePem << endl;
   //cout << "Written Public Key PEM=" << endl << outPublicPem << endl;
   
   // clean up crypto strings
   EVP_cleanup();

   tr.passIfNoException();
}

void runRsaAsymmetricKeyCreationTest(TestRunner& tr)
{
   tr.test("RSA Asymmetric Key Creation");
   
   // include crypto error strings
   ERR_load_crypto_strings();
   
   // add all algorithms
   OpenSSL_add_all_algorithms();
   
   // seed PRNG
   //RAND_load_file("/dev/urandom", 1024);
   
   // get an asymmetric key factory
   AsymmetricKeyFactory factory;
   
   // create a new key pair
   PrivateKey* privateKey;
   PublicKey* publicKey;
   factory.createKeyPair("RSA", &privateKey, &publicKey);
   
   assert(privateKey != NULL);
   assert(publicKey != NULL);
   
   // test copy constructors
   PrivateKey prvKey(*privateKey);
   PublicKey pubKey(*publicKey);
   
   // cleanup private key
   if(privateKey != NULL)
   {
      delete privateKey;
   }
   
   // cleanup public key
   if(publicKey != NULL)
   {
      delete publicKey;
   }
   
   privateKey = &prvKey;
   publicKey = &pubKey;
   
   assertStrCmp(privateKey->getAlgorithm(), "RSA");
   assertStrCmp(publicKey->getAlgorithm(), "RSA");
   
   // sign some data
   char data[] = {1,2,3,4,5,6,7,8};
   DigitalSignature* ds1 = privateKey->createSignature();
   ds1->update(data, 8);
   
   // get the signature
   char sig[ds1->getValueLength()];
   unsigned int length;
   ds1->getValue(sig, length);
   delete ds1;
   
   // verify the signature
   DigitalSignature* ds2 = publicKey->createSignature();
   ds2->update(data, 8);
   bool verified = ds2->verify(sig, length);
   delete ds2;
   
   assert(verified);
   
   string outPrivatePem =
      factory.writePrivateKeyToPem(privateKey, "password");
   string outPublicPem =
      factory.writePublicKeyToPem(publicKey);
   
   //cout << "Written Private Key PEM=" << endl << outPrivatePem << endl;
   //cout << "Written Public Key PEM=" << endl << outPublicPem << endl;
   
   // clean up crypto strings
   EVP_cleanup();

   tr.passIfNoException();
}

void runDigitalSignatureInputStreamTest(TestRunner& tr)
{
   tr.test("DigitalSignatureInputStream");
   
   // include crypto error strings
   ERR_load_crypto_strings();
   
   // add all algorithms
   OpenSSL_add_all_algorithms();
   
   // seed PRNG
   //RAND_load_file("/dev/urandom", 1024);
   
   // get an asymmetric key factory
   AsymmetricKeyFactory factory;
   
   // create a new key pair
   PrivateKey* privateKey;
   PublicKey* publicKey;
   factory.createKeyPair("RSA", &privateKey, &publicKey);
   
   assert(privateKey != NULL);
   assert(publicKey != NULL);
   
   if(privateKey != NULL && publicKey != NULL)
   {
      assertStrCmp(privateKey->getAlgorithm(), "RSA");
      assertStrCmp(publicKey->getAlgorithm(), "RSA");
      
      // sign some data
      char data[] = {1,2,3,4,5,6,7,8};
      DigitalSignature* ds1 = privateKey->createSignature();
      
      char dummy[8];
      ByteArrayInputStream bais(data, 8);
      DigitalSignatureInputStream dsos1(ds1, true, &bais, false);
      dsos1.read(dummy, 8);
      
      // get the signature
      char sig[ds1->getValueLength()];
      unsigned int length;
      ds1->getValue(sig, length);
      
      // verify the signature
      DigitalSignature* ds2 = publicKey->createSignature();
      bais.setByteArray(data, 8);
      DigitalSignatureInputStream dsos2(ds2, true, &bais, false);
      dsos2.read(dummy, 8);
      bool verified = ds2->verify(sig, length);
      
      assert(verified);
      
      string outPrivatePem =
         factory.writePrivateKeyToPem(privateKey, "password");
      string outPublicPem =
         factory.writePublicKeyToPem(publicKey);
      
      //cout << "Written Private Key PEM=" << endl << outPrivatePem << endl;
      //cout << "Written Public Key PEM=" << endl << outPublicPem << endl;
   }
   
   // cleanup private key
   if(privateKey != NULL)
   {
      delete privateKey;
   }
   
   // cleanup public key
   if(publicKey != NULL)
   {
      delete publicKey;
   }
   
   // clean up crypto strings
   EVP_cleanup();

   tr.passIfNoException();
}

void runDigitalSignatureOutputStreamTest(TestRunner& tr)
{
   tr.test("DigitalSignatureOutputStream");
   
   // include crypto error strings
   ERR_load_crypto_strings();
   
   // add all algorithms
   OpenSSL_add_all_algorithms();
   
   // seed PRNG
   //RAND_load_file("/dev/urandom", 1024);
   
   // get an asymmetric key factory
   AsymmetricKeyFactory factory;
   
   // create a new key pair
   PrivateKey* privateKey;
   PublicKey* publicKey;
   factory.createKeyPair("RSA", &privateKey, &publicKey);
   
   assert(privateKey != NULL);
   assert(publicKey != NULL);
   
   if(privateKey != NULL && publicKey != NULL)
   {
      assertStrCmp(privateKey->getAlgorithm(), "RSA");
      assertStrCmp(publicKey->getAlgorithm(), "RSA");
      
      // sign some data
      char data[] = {1,2,3,4,5,6,7,8};
      DigitalSignature* ds1 = privateKey->createSignature();
      
      ostringstream oss;
      OStreamOutputStream osos(&oss);
      DigitalSignatureOutputStream dsos1(ds1, true, &osos, false);
      dsos1.write(data, 8);
      
      // get the signature
      char sig[ds1->getValueLength()];
      unsigned int length;
      ds1->getValue(sig, length);
      
      // verify the signature
      DigitalSignature* ds2 = publicKey->createSignature();
      DigitalSignatureOutputStream dsos2(ds2, true, &osos, false);
      dsos2.write(data, 8);
      bool verified = ds2->verify(sig, length);
      
      assert(verified);
      
      string outPrivatePem =
         factory.writePrivateKeyToPem(privateKey, "password");
      string outPublicPem =
         factory.writePublicKeyToPem(publicKey);
      
      //cout << "Written Private Key PEM=" << endl << outPrivatePem << endl;
      //cout << "Written Public Key PEM=" << endl << outPublicPem << endl;
   }
   
   // cleanup private key
   if(privateKey != NULL)
   {
      delete privateKey;
   }
   
   // cleanup public key
   if(publicKey != NULL)
   {
      delete publicKey;
   }
   
   // clean up crypto strings
   EVP_cleanup();

   tr.passIfNoException();
}

void runEnvelopeTest(TestRunner& tr)
{
   tr.test("Envelope");

   // include crypto error strings
   ERR_load_crypto_strings();
   
   // add all algorithms
   OpenSSL_add_all_algorithms();
   
   // seed PRNG
   //RAND_load_file("/dev/urandom", 1024);
   
   // get an asymmetric key factory
   AsymmetricKeyFactory factory;
   
   // create a new key pair
   PrivateKey* privateKey;
   PublicKey* publicKey;
   factory.createKeyPair("RSA", &privateKey, &publicKey);

   assert(privateKey != NULL);
   assert(publicKey != NULL);
   
   if(privateKey != NULL && publicKey != NULL)
   {
      // create a secret message
      char message[] =
         "This is a confidential message. For British Eyes Only.";
      int length = strlen(message);
      
      string display1 = "";
      display1.append(message, length);
      //cout << "Sending message '" << display1 << "'" << endl;
      //cout << "Message Length=" << length << endl;
      
      // create an outgoing envelope
      SymmetricKey* secretKey;
      DigitalEnvelope* outEnv = publicKey->createEnvelope(
         "AES256", &secretKey);
      assertNoException();
      assert(outEnv != NULL);
      //cout << "Created outgoing envelope..." << endl;
      
      // update the envelope
      char output[2048];
      int outLength;
      int totalOut = 0;
      outEnv->update(message, length, output, outLength);
      //cout << "Updated outgoing envelope..." << endl;
      totalOut += outLength;
      
      // finish the envelope
      ///cout << "Output Length=" << outLength << endl;
      outEnv->finish(output + outLength, outLength);
      //cout << "Finished sealing outgoing envelope..." << endl;
      totalOut += outLength;
      
      //cout << "Total Output Length=" << totalOut << endl;
      
      // create an incoming envelope
      DigitalEnvelope* inEnv = privateKey->createEnvelope(secretKey);
      assertNoException();
      assert(inEnv != NULL);
      //cout << "Created incoming envelope..." << endl;
      
      // update the envelope
      char input[2048];
      int inLength;
      int totalIn = 0;
      inEnv->update(output, totalOut, input, inLength);
      //cout << "Updated incoming envelope..." << endl;
      totalIn += inLength;
      
      // finish the envelope
      //cout << "Input Length=" << inLength << endl;
      inEnv->finish(input + inLength, inLength);
      //cout << "Finished opening incoming envelope..." << endl;
      totalIn += inLength;
      
      //cout << "Total Input Length=" << totalIn << endl;
      
      // create a string to display the received message
      string display2 = "";
      display2.append(input, totalIn);
      
      //cout << "Received message '" << display2 << "'" << endl;

      assert(display1 == display2);
      
      // delete envelopes and key
      delete secretKey;
      delete outEnv;
      delete inEnv;
   }
   
   // cleanup private key
   if(privateKey != NULL)
   {
      delete privateKey;
   }
   
   // cleanup public key
   if(publicKey != NULL)
   {
      delete publicKey;
   }
   
   // clean up crypto strings
   EVP_cleanup();

   tr.passIfNoException();
}

void runBigIntegerTest(TestRunner& tr)
{
   tr.test("BigInteger");

   #define NSI(op, expectstr) \
   do { \
      BigInteger result = op; \
      string str; \
      result.toString(str); \
      assertStrCmp(str.c_str(), expectstr); \
   } while(0)

   BigInteger number1 = 2;
   BigInteger number2 = 123456789;
   
   assert(number1 == 2);
   assert(number2 == 123456789);

   NSI(number1, "2");
   NSI(number2, "123456789");
   NSI(number1 + number2, "123456791");
   NSI(number1 - number2, "-123456787");
   NSI(number1 * number2, "246913578");
   NSI(number2 / number1, "61728394");
   NSI(number2 % number1, "1");
   NSI(number2.pow(number1), "15241578750190521");

   #undef NSI

   tr.passIfNoException();
}

void runBigDecimalTest(TestRunner& tr)
{
   tr.test("BigDecimal");
   
   #define NSD(op, expectstr) \
   do { \
      BigDecimal result = op; \
      string str; \
      result.toString(str); \
      assertStrCmp(str.c_str(), expectstr); \
   } while(0)

   BigDecimal number1 = 3.0;
   //BigDecimal number2 = 123456789.5;
   BigDecimal number2 = "123456789.53";
   //BigDecimal number2 = 1.234;
   //BigDecimal number2 = "1.23e-04";
   //BigDecimal number2 = "1234";
      
   NSD(number1, "3");
   NSD(number2, "123456789.53");
   NSD(number1 + number2, "123456792.53");
   NSD(number1 - number2, "-123456786.53");
   NSD(number1 * number2, "370370368.59");
   NSD(number2 / number1, "41152263.1766666667");
   NSD(number2 % number1, "0.53");

   #define NSDR(n, i, d, expectstr) \
   do { \
      BigDecimal nr = n; \
      nr.setPrecision(i, d); \
      nr.round(); \
      NSD(nr, expectstr); \
   } while(0)
   
   BigDecimal number3 = "129.54678";
   NSD(number3, "129.54678");
   
   NSDR(number3, 7, Up, "129.54678");
   NSDR(number3, 6, Up, "129.54678");
   NSDR(number3, 5, Up, "129.54678");
   NSDR(number3, 4, Up, "129.5468");
   NSDR(number3, 3, Up, "129.547");
   NSDR(number3, 2, Up, "129.55");
   NSDR(number3, 1, Up, "129.6");
   NSDR(number3, 0, Up, "130");

   NSDR(number3, 7, HalfUp, "129.54678");
   NSDR(number3, 6, HalfUp, "129.54678");
   NSDR(number3, 5, HalfUp, "129.54678");
   NSDR(number3, 4, HalfUp, "129.5468");
   NSDR(number3, 3, HalfUp, "129.547");
   NSDR(number3, 2, HalfUp, "129.55");
   NSDR(number3, 1, HalfUp, "129.5");
   NSDR(number3, 0, HalfUp, "130");

   NSDR(number3, 7, Down, "129.54678");
   NSDR(number3, 6, Down, "129.54678");
   NSDR(number3, 5, Down, "129.54678");
   NSDR(number3, 4, Down, "129.5467");
   NSDR(number3, 3, Down, "129.546");
   NSDR(number3, 2, Down, "129.54");
   NSDR(number3, 1, Down, "129.5");
   NSDR(number3, 0, Down, "129");

   /*
   BigDecimal bd;
   
   for(int i = 7; i >= 0; i--)
   {
      bd = number3;
      bd.setPrecision(i, Up);
      bd.round();
      cout << "round " << i << " places, up=" << bd << endl;
   }
   
   for(int i = 7; i >= 0; i--)
   {
      bd = number3;
      bd.setPrecision(i, HalfUp);
      bd.round();
      cout << "round " << i << " places, half up=" << bd << endl;
   }
   
   for(int i = 7; i >= 0; i--)
   {
      bd = number3;
      bd.setPrecision(i, Down);
      bd.round();
      cout << "round " << i << " places, down=" << bd << endl;
   }
   */

   #undef NSD
   #undef NSDR
   
   tr.passIfNoException();
}

class DbCryptoTester : public db::test::Tester
{
public:
   DbCryptoTester()
   {
      setName("crypto");
   }

   /**
    * Run automatic unit tests.
    */
   virtual int runAutomaticTests(TestRunner& tr)
   {
      runMessageDigestTest(tr);
      runCipherTest(tr, "AES256");
      runAsymmetricKeyLoadingTest(tr);
      runDsaAsymmetricKeyCreationTest(tr);
      runRsaAsymmetricKeyCreationTest(tr);
      runDigitalSignatureInputStreamTest(tr);
      runDigitalSignatureOutputStreamTest(tr);
      runEnvelopeTest(tr);
      runBigIntegerTest(tr);
      runBigDecimalTest(tr);
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
DB_TEST_MAIN(DbCryptoTester)
#endif