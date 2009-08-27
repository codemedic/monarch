/*
 * Copyright (c) 2007-2009 Digital Bazaar, Inc. All rights reserved.
 */
#include "db/io/File.h"

#include "db/io/FileFunctions.h"
#include "db/io/FileList.h"
#include "db/io/FileInputStream.h"
#include "db/io/FileOutputStream.h"
#include "db/rt/DynamicObject.h"
#include "db/util/StringTokenizer.h"
#include "db/util/StringTools.h"

#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <vector>

using namespace std;
using namespace db::io;
using namespace db::rt;
using namespace db::util;

#ifdef WIN32
   const char* File::NAME_SEPARATOR = "\\";
   const char File::NAME_SEPARATOR_CHAR = '\\';
   const char* File::PATH_SEPARATOR = ";";
   const char File::PATH_SEPARATOR_CHAR = ';';

   // a helper function for stripping drive letters from windows paths
   static string stripDriveLetter(const char* path, string* drive = NULL)
   {
      string rval = path;

      if(drive != NULL)
      {
         drive->erase();
      }

      if(File::isPathAbsolute(path))
      {
         if(path[0] != 0 && path[1] != 0)
         {
            rval = (path + 2);
            if(drive != NULL)
            {
               drive->append(path, 2);
            }
         }
      }

      return rval;
   }

   // a helper function to replace '/' slashes with '\'
   static string flipSlashes(const char* path)
   {
      // transform '/' to '\' (no need to worry about transforming '/' that
      // are actually supposed to be '/' in a windows path because those
      // characters are illegal in a windows path
      string tmp = path;
      StringTools::replaceAll(tmp, "/", "\\");
      return tmp;
   }
#else
   const char* File::NAME_SEPARATOR = "/";
   const char File::NAME_SEPARATOR_CHAR = '/';
   const char* File::PATH_SEPARATOR = ":";
   const char File::PATH_SEPARATOR_CHAR = ':';
#endif

FileImpl::FileImpl() :
   mRemoveOnCleanup(false)
{
   mPath = strdup(".");

   // initialize absolute path
   string abs;
   File::getAbsolutePath(mPath, abs);
   mAbsolutePath = strdup(abs.c_str());

   mBaseName = mCanonicalPath = mExtension = NULL;
}

FileImpl::FileImpl(const char* path) :
   mRemoveOnCleanup(false)
{
#ifdef WIN32
   mPath = strdup(flipSlashes(path).c_str());
#else
   // just copy path
   mPath = strdup(path);
#endif

   // initialize absolute path
   string abs;
   File::getAbsolutePath(mPath, abs);
   mAbsolutePath = strdup(abs.c_str());

   mBaseName = mCanonicalPath = mExtension = NULL;
}

FileImpl::~FileImpl()
{
   if(mRemoveOnCleanup)
   {
      // remove temp file
      remove();
   }

   free(mPath);
   free(mAbsolutePath);

   if(mBaseName != NULL)
   {
      free(mBaseName);
   }

   if(mCanonicalPath != NULL)
   {
      free(mCanonicalPath);
   }

   if(mExtension != NULL)
   {
      free(mExtension);
   }
}

bool FileImpl::create()
{
   bool rval = false;

   FILE* fp = fopen(mAbsolutePath, "w");
   if(fp != NULL)
   {
      rval = true;
      fclose(fp);
   }
   else
   {
      ExceptionRef e = new Exception(
         "Could not create file",
         "db.io.File.CreateFailed");
      e->getDetails()["path"] = mAbsolutePath;
      e->getDetails()["error"] = strerror(errno);
      Exception::set(e);
   }

   return rval;
}

bool FileImpl::mkdirs()
{
   bool rval = true;

   // get path
   string path = (isDirectory() ?
      mAbsolutePath : File::parentname(mAbsolutePath));

   // create stack of directories in the path
   vector<string> dirStack;
   while(!File::isPathRoot(path.c_str()))
   {
      dirStack.push_back(path);
      path = File::parentname(path.c_str());
   }

   // iteratively create directories
   struct stat s;
   int rc;
   while(rval && !dirStack.empty())
   {
      path = dirStack.back();
      dirStack.pop_back();

      // try to stat directory
      rc = stat(path.c_str(), &s);
      if(rc != 0)
      {
         // directory doesn't exist, so try to create it
         // Note: windows ignores permissions in mkdir(), always 0777
         if(mkdir(path.c_str(), 0777) < 0)
         {
            ExceptionRef e = new Exception(
               "Could not create directory.",
               "db.io.File.CreateDirectoryFailed");
            e->getDetails()["path"] = path.c_str();
            e->getDetails()["error"] = strerror(errno);
            Exception::set(e);
            rval = false;
         }
      }
   }

   return rval;
}

bool FileImpl::exists()
{
   bool rval = false;

   struct stat s;
   int rc = stat(mAbsolutePath, &s);
   if(rc == 0)
   {
      rval = true;
   }
   else
   {
      // does not set an exception intentionally, the file just doesn't exist
   }

   return rval;
}

bool FileImpl::remove()
{
   bool rval = false;

   int rc = ::remove(mAbsolutePath);
   if(rc == 0)
   {
      rval = true;
   }
   else if(exists())
   {
      // only set exception when the file exists and could not be removed
      ExceptionRef e = new Exception(
         "Could not delete file.",
         "db.io.File.DeleteFailed");
      e->getDetails()["path"] = mAbsolutePath;
      e->getDetails()["error"] = strerror(errno);
      Exception::set(e);
   }

   return rval;
}

void FileImpl::setRemoveOnCleanup(bool remove)
{
   mRemoveOnCleanup = remove;
}

bool FileImpl::rename(File& file)
{
   bool rval = false;

   // delete old file
   file->remove();

   // rename file
   int rc = ::rename(mAbsolutePath, file->getAbsolutePath());
   if(rc == 0)
   {
      rval = true;
   }
   else
   {
      ExceptionRef e = new Exception(
         "Could not rename file.",
         "db.io.File.RenameFailed");
      e->getDetails()["oldName"] = mAbsolutePath;
      e->getDetails()["newName"] = file->getAbsolutePath();
      e->getDetails()["error"] = strerror(errno);
      Exception::set(e);
   }

   return rval;
}

const char* FileImpl::getBaseName()
{
   if(mBaseName == NULL)
   {
      // initialize base name
      mBaseName = strdup(File::basename(mAbsolutePath).c_str());
   }

   return mBaseName;
}

const char* FileImpl::getPath() const
{
   return mPath;
}

const char* FileImpl::getAbsolutePath() const
{
   return mAbsolutePath;
}

const char* FileImpl::getCanonicalPath()
{
   if(mCanonicalPath == NULL)
   {
      string path;
      File::getCanonicalPath(mAbsolutePath, path);
      mCanonicalPath = strdup(path.c_str());
   }

   return mCanonicalPath;
}

const char* FileImpl::getExtension()
{
   if(mExtension == NULL)
   {
      string root;
      string ext;
      File::splitext(mAbsolutePath, root, ext);
      mExtension = strdup(ext.c_str());
   }

   return mExtension;
}

off_t FileImpl::getLength()
{
   struct stat s;
   int rc = stat(mAbsolutePath, &s);
   if(rc != 0)
   {
      ExceptionRef e = new Exception(
         "Could not stat file.",
         "db.io.File.StatFailed");
      e->getDetails()["path"] = mAbsolutePath;
      e->getDetails()["error"] = strerror(errno);
      Exception::set(e);
   }

   return s.st_size;
}

FileImpl::Type FileImpl::getType(bool follow)
{
   Type rval = Unknown;

   struct stat s;
   int rc;

   if(follow)
   {
      // ensure follow links are followed with stat
      rc = stat(mAbsolutePath, &s);
   }
   else
   {
      // use lstat so symbolic links aren't followed
      rc = lstat(mAbsolutePath, &s);
   }

   if(rc == 0)
   {
      switch(s.st_mode & S_IFMT)
      {
         case S_IFREG:
            rval = RegularFile;
            break;
         case S_IFDIR:
            rval = Directory;
            break;
         case S_IFLNK:
            rval = SymbolicLink;
            break;
         default:
            break;
      }
   }

   return rval;
}

bool FileImpl::isFile()
{
   return getType() == RegularFile;
}

bool FileImpl::contains(const char* path)
{
   bool rval = false;

   string containee;
   if(File::getAbsolutePath(path, containee))
   {
      rval = (containee.find(mAbsolutePath, 0) == 0);
   }

   return rval;
}

bool FileImpl::contains(File& path)
{
   return contains(path->getAbsolutePath());
}

bool FileImpl::isDirectory()
{
   return getType() == Directory;
}

bool FileImpl::isRoot()
{
   return File::isPathRoot(mAbsolutePath);
}

bool FileImpl::isReadable()
{
   return File::isPathReadable(mAbsolutePath);
}

bool FileImpl::isSymbolicLink()
{
   return getType(false) == SymbolicLink;
}

bool FileImpl::isWritable()
{
   return File::isPathWritable(mAbsolutePath);
}

void FileImpl::listFiles(FileList& files)
{
   if(isDirectory())
   {
      // open directory
      DIR* dir = opendir(mAbsolutePath);
      if(dir == NULL)
      {
         // FIXME: add error handling
      }
      else
      {
         // read each directory entry
         struct dirent* entry;
         unsigned int len1 = strlen(mAbsolutePath);
         bool separator = mAbsolutePath[len1 - 1] != File::NAME_SEPARATOR_CHAR;
         while((entry = readdir(dir)) != NULL)
         {
            // d_name is null-terminated name for file, without path name
            // so copy file name before d_name to get full path
            unsigned int len2 = strlen(entry->d_name);
            char path[len1 + len2 + 2];
            memcpy(path, mAbsolutePath, len1);
            if(separator)
            {
               // add path separator as appropriate
               path[len1] = File::NAME_SEPARATOR_CHAR;
               memcpy(path + len1 + 1, entry->d_name, len2 + 1);
            }
            else
            {
               memcpy(path + len1, entry->d_name, len2 + 1);
            }

            // add new file to list
            File file(path);
            files->add(file);
         }

         // close directory
         closedir(dir);
      }
   }
}

Date FileImpl::getModifiedDate()
{
   Date date(0);

   struct stat s;
   if(stat(mAbsolutePath, &s) == 0)
   {
      date.setSeconds(s.st_mtime);
   }

   return date;
}

bool File::operator==(const File& rhs) const
{
   bool rval = false;

   File& file = *((File*)&rhs);

   // compare absolute paths and types for equality,
   // use case-insensitive compare for windows
#ifdef WIN32
   if(strcasecmp((*this)->getAbsolutePath(), file->getAbsolutePath()) == 0)
#else
   if(strcmp((*this)->getAbsolutePath(), file->getAbsolutePath()) == 0)
#endif
   {
      rval = ((*this)->getType() == file->getType());
   }

   return rval;
}

bool File::operator!=(const File& rhs) const
{
   return !operator==(rhs);
}

bool File::readBytes(ByteBuffer* buffer)
{
   bool rval = true;

   // read into the buffer until full (error, not enough space),
   // or until EOF (success)
   FileInputStream* fis = new FileInputStream(*this);
   int numBytes = -1;
   while(!buffer->isFull() &&
         (numBytes = fis->read(buffer->end(), buffer->freeSpace())) > 0)
   {
      // extend buffer by amount read
      buffer->extend(numBytes);
   }
   fis->close();
   delete fis;

   if(numBytes == -1)
   {
      rval = false;
   }
   else if(numBytes != 0 && buffer->isFull())
   {
      ExceptionRef e = new Exception(
         "Could not read entire file. Buffer is full.",
         "db.io.File.InsufficientBufferSpace");
      Exception::set(e);
      rval = false;
   }

   return rval;
}

bool File::writeBytes(ByteBuffer* buffer, bool append)
{
   bool rval = true;

   // write entire buffer to file
   FileOutputStream* fos = new FileOutputStream(*this, append);
   rval = fos->write(buffer->data(), buffer->length());
   fos->close();
   delete fos;

   return rval;
}

bool File::getAbsolutePath(const char* path, string& absolutePath)
{
   bool rval = true;

#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   // if the path isn't absolute, prepend the current working directory
   // to the path
   string tmp;
   if(!isPathAbsolute(path))
   {
      string cwd;
      if((rval = getCurrentWorkingDirectory(cwd)))
      {
         tmp = File::join(cwd.c_str(), path);
      }
   }
   else
   {
      // path already absolute
      tmp.assign(path);
   }

   // normalize path
   rval = rval && normalizePath(tmp.c_str(), absolutePath);

   return rval;
}

bool File::getCanonicalPath(const char* path, string& canonicalPath)
{
   bool rval = true;

#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   // FIXME: add while loop to keep following symbolic links after
   // getting absolute path
   // FIXME: call readlink(path, outputbuffer, outputbuffersize), returns
   // number of characters in buffer or -1 with errno set

   // get absolute path
   rval = getAbsolutePath(path, canonicalPath);

   return rval;
}

bool File::normalizePath(const char* path, string& normalizedPath)
{
   bool rval = true;

#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();

   // strip drive letter, point "path" at stripped path
   string drive;
   string stripped = stripDriveLetter(path, &drive);
   path = stripped.c_str();
#endif

   string tempPath;
   if(strlen(path) > 0)
   {
      // store whether or not path begins with path name separator
      bool separator = (path[0] == NAME_SEPARATOR_CHAR);

      // clean up the relative directory references, by traversing the
      // path in reverse
      StringTokenizer st(path, NAME_SEPARATOR_CHAR, false);
      int skip = 0;
      while(st.hasPreviousToken())
      {
         const char* token = st.previousToken();
         if(strcmp(token, "..") == 0)
         {
            // since we're traversing the path backwards, skip the upcoming
            // previous directory because we found a ".."
            skip++;
         }
         else if(strcmp(token, ".") != 0)
         {
            if(skip == 0)
            {
               // not skipping directory, so join to the normalized path
               tempPath = File::join(token, tempPath.c_str());
            }
            else
            {
               // directory skipped
               skip--;
            }
         }
      }

      // re-insert path name separator
      if(separator)
      {
         tempPath.insert(0, 1, NAME_SEPARATOR_CHAR);
      }

      if(tempPath.length() == 0 || (skip > 0 && !isPathAbsolute(path)))
      {
         ExceptionRef e = new Exception(
            "Could not normalize relative path.",
            "db.io.File.BadNormalization");
         e->getDetails()["path"] = path;
         Exception::set(e);
         rval = false;
      }
   }

#ifdef WIN32
   // re-add drive letter before assigning to temp path
   normalizedPath.assign(drive);
   normalizedPath.append(tempPath);
#else
   // assign to temp path
   normalizedPath.assign(tempPath);
#endif

   return rval;
}

bool File::expandUser(const char* path, string& expandedPath)
{
   bool rval = true;

#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   size_t pathlen = 0;
   if(path != NULL)
   {
      pathlen = strlen(path);
   }

   // UNIX-like platforms:
   //    expand "~" to $HOME
   // Windows:
   //    expand "~" to %USERPROFILE%
   //    expand "%USERPROFILE%"
   //    expand "%HOMEDRIVE%", "%HOMEPATH%", and "%HOMEDRIVE%%HOMEPATH%"
   // All:
   //    No support for ~username yet. Only ~ and ~/... supported.
   bool expandTilde = false;

   if(pathlen > 0 && path[0] == '~')
   {
      // FIXME add getpwnam support
      // only handle current user right now
      if(pathlen > 1 && path[1] != NAME_SEPARATOR_CHAR)
      {
         ExceptionRef e = new Exception(
            "Only current user supported (ie, \"~/...\").",
            "db.io.File.NotImplemented");
         Exception::set(e);
         rval = false;
      }
      else
      {
         expandTilde = true;
      }
   }

   if(rval)
   {
      // default to append full initial path
      string::size_type pathOffset = 0;
      // Use temp string to avoid problems if path is same expandedPath
      // common for code like expandUser(path.c_str(), path)
      string newPath;
#ifdef WIN32
      #define HD "%HOMEDRIVE%"
      #define HDLEN 11
      #define HP "%HOMEPATH%"
      #define HPLEN 10
      #define UP "%USERPROFILE%"
      #define UPLEN 13
      #define HDHP HD HP
      #define HDHPLEN (HDLEN + HPLEN)

      // Note: Only the first occurrences of ~, HOMEDRIVE, HOMEPATH, or
      // USERPROFILE will be replaced. If HOMEDRIVE and HOMEPATH both occur,
      // then both of their first occurrences will be replaced. If there are
      // other occurrences, then the path will be invalid on windows anyway
      // ... so we don't bother replacing them here.

      if(expandTilde)
      {
         const char* userProfile = getenv("USERPROFILE");
         if(userProfile == NULL)
         {
            // no USERPROFILE set
            ExceptionRef e = new Exception(
               "No USERPROFILE environment variable set for "
               "'%USERPROFILE%' expansion.",
               "db.io.File.UserProfileNotSet");
            Exception::set(e);
            rval = false;
         }
         else
         {
            // add USERPFOFILE
            newPath.append(userProfile);
            // offset to after '~'
            pathOffset = 1;
         }
      }
      else if(pathlen >= UPLEN && strncmp(path, UP, UPLEN) == 0)
      {
         const char* userProfile = getenv("USERPROFILE");
         if(userProfile == NULL)
         {
            // no USERPROFILE set
            ExceptionRef e = new Exception(
               "No USERPROFILE environment variable set for "
               "'%USERPROFILE%' expansion.",
               "db.io.File.UserProfileNotSet");
            Exception::set(e);
            rval = false;
         }
         else
         {
            // add USERPFOFILE
            newPath.append(userProfile);
            // offset after USERPROFILE* token
            pathOffset = UPLEN;
         }
      }
      else if(pathlen >= HDHPLEN && strncmp(path, HDHP, HDHPLEN) == 0)
      {
         const char* homeDrive = getenv("HOMEDRIVE");
         const char* homePath = getenv("HOMEPATH");
         if(homeDrive == NULL)
         {
            // no HOMEDRIVE set
            ExceptionRef e = new Exception(
               "No HOMEDRIVE environment variable set for "
               "'%HOMEDRIVE%' expansion.",
               "db.io.File.HomeDriveNotSet");
            Exception::set(e);
            rval = false;
         }
         if(rval && homePath == NULL)
         {
            // no HOMEPATH set
            ExceptionRef e = new Exception(
               "No HOMEPATH environment variable set for "
               "'%HOMEPATH%' expansion.",
               "db.io.File.HomePathNotSet");
            Exception::set(e);
            rval = false;
         }
         if(rval)
         {
            // add HOMEDRIVE
            newPath.append(homeDrive);
            // add HOMEPATH
            newPath.append(homePath);
            // offset after HOME* tokens
            pathOffset = HDHPLEN;
         }
      }
      else if(pathlen >= HDLEN && strncmp(path, HD, HDLEN) == 0)
      {
         const char* homeDrive = getenv("HOMEDRIVE");
         if(homeDrive != NULL)
         {
            // add HOME
            newPath.append(homeDrive);
            // offset after HOME* tokens
            pathOffset = HDLEN;
         }
         else
         {
            // no HOMEDRIVE set
            ExceptionRef e = new Exception(
               "No HOMEDRIVE environment variable set for "
               "'%HOMEDRIVE%' expansion.",
               "db.io.File.HomeDroveNotSet");
            Exception::set(e);
            rval = false;
         }
      }
      else if(pathlen >= HPLEN && strncmp(path, HP, HPLEN) == 0)
      {
         const char* homePath = getenv("HOMEPATH");
         if(homePath != NULL)
         {
            // add HOME
            newPath.append(homePath);
            // offset after HOME* tokens
            pathOffset = HPLEN;
         }
         else
         {
            // no HOME set
            ExceptionRef e = new Exception(
               "No HOMEPATH environment variable set for "
               "'%HOMEPATH%' expansion.",
               "db.io.File.HomePathNotSet");
            Exception::set(e);
            rval = false;
         }
      }
#else
      if(expandTilde)
      {
         const char* home = getenv("HOME");
         if(home != NULL)
         {
            // add HOME
            newPath.append(home);
            // offset after '~'
            pathOffset = 1;
         }
         else
         {
            // no HOME set
            ExceptionRef e = new Exception(
               "No HOME environment variable set for '~' expansion.",
               "db.io.File.HomeNotSet");
            Exception::set(e);
            rval = false;
         }
      }
#endif
      if(rval)
      {
         // add rest of path after expanded tokens
         newPath.append(path + pathOffset);
         // copy to output
         expandedPath.assign(newPath);
      }
   }

   return rval;
}

bool File::getCurrentWorkingDirectory(string& cwd)
{
   bool rval = true;

   bool found = false;
   size_t path_max;
#ifdef PATH_MAX
   path_max = PATH_MAX;
#else
   path_max = 1024;
#endif

   char* b = (char*)malloc(path_max);
   while(rval && !found)
   {
      if(getcwd(b, path_max) != NULL)
      {
         cwd.assign(b);
         found = true;
      }
      else
      {
         // not enough space for path
         // fail to check again if bigger than arbitrary size of 8k
         if(errno == ERANGE && path_max < (1024 * 8))
         {
            path_max *= 2;
            b = (char*)realloc(b, path_max);
         }
         else
         {
            // path was too large for getcwd
            ExceptionRef e = new Exception(
               "Could not get current working directory, path too long.",
               "db.io.File.PathTooLong");
            Exception::set(e);
            rval = false;
         }
      }
   }
   free(b);

   return rval;
}

bool File::getTemporaryDirectory(string& tmp)
{
   bool rval = true;

   char* tmpdir = getenv("TMPDIR");
   if(tmpdir == NULL)
   {
#ifdef WIN32
      TCHAR path[MAX_PATH];
      GetTempPath(MAX_PATH, path);
      tmp = path;
#else
      tmp = "/tmp";
#endif
   }

   return rval;
}

File File::createTempFile(const char* prefix, const char* dir)
{
   File rval((FileImpl*)NULL);

   string tmp;
   if(dir != NULL)
   {
      tmp = dir;
   }
   if(dir != NULL || getTemporaryDirectory(tmp))
   {
      int fd = -1;
      char* path = NULL;
      char num[22];
      string filename;
      uint64_t time;
      for(int i = 0; i < TMP_MAX; i++)
      {
         // try to get unique temporary path name
         time = System::getCurrentMilliseconds();
         snprintf(num, 22, "%llu", time % 1000);
         num[4] = 0;
         for(int n = 0; num[n] != 0; n++)
         {
            num[n] += ('A' - '0');
         }
         path = tempnam(tmp.c_str(), num);
         if(path != NULL)
         {
            // add prefix
            string dirname;
            string basename;
            File::split(path, dirname, basename);
            basename.insert(0, prefix);
            filename = File::join(dirname.c_str(), basename.c_str());
            free(path);

            // try to uniquely open the file
            fd = open(filename.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
            if(fd == -1)
            {
               filename.erase();
               if(errno == EEXIST)
               {
                  // file already exists, try again
                  continue;
               }
               else
               {
                  // other file error
                  break;
               }
            }
            else
            {
               // file created and unique, close it
               close(fd);
               break;
            }
         }
      }

      if(fd == -1)
      {
         ExceptionRef e = new Exception(
            "Could not create temp file.",
            "db.io.File.CreateTempFileFailed");
         if(filename.length() > 0)
         {
            e->getDetails()["path"] = filename.c_str();
            free(path);
         }
         e->getDetails()["error"] = strerror(errno);
         Exception::set(e);
      }
      else
      {
         FileImpl* impl = new FileImpl(filename.c_str());
         impl->setRemoveOnCleanup(true);
         rval = impl;
      }
   }

   return rval;
}

bool File::isPathReadable(const char* path)
{
#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   return (access(path, R_OK) == 0);
}

bool File::isPathWritable(const char* path)
{
#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   return (access(path, W_OK) == 0);
}

void File::split(const char* path, string& dirname, string& basename)
{
#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   if(isPathRoot(path))
   {
      // root path has no basename
      dirname = path;
      basename.erase();
   }
   else
   {
      // split on the last path name separator
      char* pos = strrchr(path, NAME_SEPARATOR_CHAR);
      if(pos != NULL)
      {
         pos++;
         dirname.assign(path, (pos - path));
         basename.assign(pos);
      }
      else
      {
         dirname.erase();
         basename.assign(path);
      }
   }

   // strip trailing slashes from dirname
   if(dirname.length() > 1)
   {
      dirname.erase(dirname.find_last_not_of(NAME_SEPARATOR_CHAR) + 1);
      if(dirname.length() == 0)
      {
         dirname.assign(NAME_SEPARATOR);
      }
   }
}

void File::splitext(
   const char* path, string& root, string& ext, const char* sep)
{
#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   string sPath = path;
   string::size_type pos = sPath.rfind(sep);
   if(pos != string::npos)
   {
      root.assign(sPath.substr(0, pos));
      ext.assign(sPath.substr(pos));
   }
   else
   {
      root.assign(path);
      ext.clear();
   }
}

string File::parentname(const char* path)
{
#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path);
   path = tmp1.c_str();
#endif

   string rval = path;

   // check to see if the path is a root
   if(!isPathRoot(path))
   {
      // path is NOT a root:
      // strip trailing slashes from path before getting parent
      rval.erase(rval.find_last_not_of(NAME_SEPARATOR_CHAR) + 1);
      rval = File::dirname(rval.c_str());
   }

   return rval;
}

string File::dirname(const char* path)
{
   string dirname;
   string basename;

   File::split(path, dirname, basename);

   return dirname;
}

string File::basename(const char* path)
{
   string dirname;
   string basename;

   File::split(path, dirname, basename);

   return basename;
}

bool File::isPathAbsolute(const char* path)
{
   bool rval = false;

   if(path != NULL)
   {
#ifdef WIN32
      // handle windows slashes mess
      string tmp1 = flipSlashes(path);
      path = tmp1.c_str();

      // absolute paths on windows start with:
      // "\" OR
      // "<drive letter>:" OR
      // "<drive letter>:\"
      size_t len = strlen(path);
      if(len >= 1 && path[0] == NAME_SEPARATOR_CHAR)
      {
         rval = true;
      }
      else if(len >= 2 && path[1] == ':')
      {
         char drive = path[0];
         if((drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z'))
         {
            if(len == 2 || (len > 2 && path[2] == NAME_SEPARATOR_CHAR))
            {
               rval = true;
            }
         }
      }
#else
      // just compare against name separator
      rval = (path != NULL && path[0] == NAME_SEPARATOR_CHAR);
#endif
   }

   return rval;
}

bool File::isPathRoot(const char* path)
{
   bool rval = false;

   if(path != NULL)
   {
      // Note: We cannot just check to see if the absolute path is the same as
      // the parent because that would require making calls to parentname & split
      // which rely on this method -- hence a circular dependency would be
      // introduced. Therefore this code must be uglier.
#ifdef WIN32
      // handle windows slashes mess
      string tmp1 = flipSlashes(path);
      path = tmp1.c_str();

      // root paths for windows must be:
      // "\" OR
      // "<drive letter>:" OR
      // "<drive letter>:\"
      size_t len = strlen(path);
      if(len == 1 && path[0] == NAME_SEPARATOR_CHAR)
      {
         rval = true;
      }
      else if((len == 2 || len == 3) && path[1] == ':')
      {
         char drive = path[0];
         if((drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z'))
         {
            if(len == 2 || path[2] == NAME_SEPARATOR_CHAR)
            {
               rval = true;
            }
         }
      }
#else
      // just compare against name separator
      rval = (path != NULL && strcmp(path, NAME_SEPARATOR) == 0);
#endif
   }

   return rval;
}

string File::join(const char* path1, const char* path2)
{
#ifdef WIN32
   // handle windows slashes mess
   string tmp1 = flipSlashes(path1);
   string tmp2 = flipSlashes(path2);
   path1 = tmp1.c_str();
   path2 = tmp2.c_str();
#endif

   // start with path1
   string path = path1;

   // skip if path2 empty
   if(strlen(path2) > 0)
   {
      string::size_type plen = path.length();
      if(plen == 0)
      {
         // empty path1 so just assign path2
         path.assign(path2);
      }
      else
      {
         bool path1HasSep = (path[plen - 1] == NAME_SEPARATOR_CHAR);
         bool path2HasSep = (path2[0] == NAME_SEPARATOR_CHAR);
         if(!path1HasSep && !path2HasSep)
         {
            // no trailing path1 separator or leading path2 separator
            path.push_back(NAME_SEPARATOR_CHAR);
            path.append(path2);
         }
         else if(path1HasSep && path2HasSep)
         {
            // trailing and leading slash, skip one
            path.append(path2 + 1);
         }
         else
         {
            // only one of trailing or leading, just append
            path.append(path2);
         }
      }
   }

   return path;
}