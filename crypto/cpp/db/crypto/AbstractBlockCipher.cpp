/*
 * Copyright (c) 2007-2009 Digital Bazaar, Inc. All rights reserved.
 */
#include "db/crypto/AbstractBlockCipher.h"

#include "db/rt/Exception.h"

#include <cstring>

using namespace db::crypto;
using namespace db::rt;

AbstractBlockCipher::AbstractBlockCipher(bool encrypt)
{
   // store encrypt mode
   mEncryptMode = encrypt;
   
   // initialize input/output bytes
   mInputBytes = mOutputBytes = 0;
   
   // initialize the cipher context
   EVP_CIPHER_CTX_init(&mCipherContext);
   
   // set the cipher function to null
   mCipherFunction = NULL;
}

AbstractBlockCipher::~AbstractBlockCipher()
{
   // clean up the cipher context
   EVP_CIPHER_CTX_cleanup(&mCipherContext);
}

const EVP_CIPHER* AbstractBlockCipher::getCipherFunction(const char* algorithm)
{
   const EVP_CIPHER* rval = NULL;
   
   if(strcmp(algorithm, "AES") == 0 || strcmp(algorithm, "AES256") == 0)
   {
      rval = EVP_aes_256_cbc();
   }
   else if(strcmp(algorithm, "AES128") == 0)
   {
      rval = EVP_aes_128_cbc();
   }
   else if(strcmp(algorithm, "3DES") == 0)
   {
      rval = EVP_des_ede3_cbc();
   }
   else
   {
      ExceptionRef e = new Exception(
         "Unsupported key algorithm", "db.crypto.UnsupportedAlgorithm");
      e->getDetails()["algorithm"] = algorithm;
      Exception::set(e);
   }
   
   return rval;
}

unsigned int AbstractBlockCipher::getBlockSize()
{
   return EVP_CIPHER_CTX_block_size(&mCipherContext);
}

bool AbstractBlockCipher::isEncryptEnabled()
{
   return mEncryptMode;
}
