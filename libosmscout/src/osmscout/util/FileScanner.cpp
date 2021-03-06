/*
  This source is part of the libosmscout library
  Copyright (C) 2009  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <osmscout/private/Config.h>

#include <osmscout/util/FileScanner.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <limits>

#if defined(HAVE_MMAP)
  #include <unistd.h>
  #include <sys/mman.h>
#endif

#if defined(HAVE_FCNTL_H)
  #include <fcntl.h>
#endif

#if defined(__WIN32__) || defined(WIN32)
  #include<io.h>

  #if !defined(_fileno)
    #define _fileno(__F) ((__F)->_file)
  #endif
#endif

#include <osmscout/system/Assert.h>

#include <osmscout/util/Logger.h>
#include <osmscout/util/Number.h>

namespace osmscout {

  FileScanner::FileScanner()
   : file(NULL),
     hasError(true),
     buffer(NULL),
     size(0),
     offset(0),
     byteBuffer(NULL),
     byteBufferSize(0)
#if defined(__WIN32__) || defined(WIN32)
     ,mmfHandle((HANDLE)0)
#endif
  {
    // no code
  }

  FileScanner::~FileScanner()
  {
    if (IsOpen()) {
      log.Warn() << "Automatically closing FileScanner for file '" << filename << "'!";
      Close();
    }

    delete [] byteBuffer;
  }

  void FileScanner::AssureByteBufferSize(size_t size)
  {
    if (byteBufferSize>=size) {
      return;
    }

    delete [] byteBuffer;

    byteBuffer=new uint8_t[size];
    byteBufferSize=size;

  }

  void FileScanner::FreeBuffer()
  {
#if defined(HAVE_MMAP)
    if (buffer!=NULL) {
      if (munmap(buffer,size)!=0) {
        log.Error() << "Error while calling munmap: "<< strerror(errno) << " for file '" << filename << "'";
      }

      buffer=NULL;
    }
#elif  defined(__WIN32__) || defined(WIN32)
      if (buffer!=NULL) {
        UnmapViewOfFile(buffer);
        buffer=NULL;
      }
      if (mmfHandle!=NULL) {
        CloseHandle(mmfHandle);
        mmfHandle=NULL;
      }
#endif
  }

  bool FileScanner::Open(const std::string& filename,
                         Mode mode,
                         bool useMmap)
  {
    if (file!=NULL) {
      log.Error() << "File '" << filename << "' already opened, cannot open it again!";
      return false;
    }

    this->filename=filename;

    file=fopen(filename.c_str(),"rb");

    if (file==NULL) {
      hasError=true;
      return false;
    }

#if defined(HAVE_FSEEKO)
    off_t size;

    if (fseeko(file,0L,SEEK_END)!=0) {
      log.Error() << "Cannot seek to end of file '" << filename << "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    size=ftello(file);

    if (size==-1) {
      log.Error() << "Cannot get size of file '" << filename << "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    this->size=(FileOffset)size;

    if (fseeko(file,0L,SEEK_SET)!=0) {
      log.Error() << "Cannot seek to start of file '" << filename << "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }
#else
    long size;

    if (fseek(file,0L,SEEK_END)!=0) {
      log.Error() << "Cannot seek to end of file '" << filename << "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    size=ftell(file);

    if (size==-1) {
      log.Error() << "Cannot get size of file '" << filename << "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    this->size=(FileOffset)size;

    if (fseek(file,0L,SEEK_SET)!=0) {
      log.Error() << "Cannot seek to start of file '" << filename << "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }
#endif

#if defined(HAVE_POSIX_FADVISE)
    if (mode==FastRandom) {
      if (posix_fadvise(fileno(file),0,size,POSIX_FADV_WILLNEED)<0) {
        log.Error() << "Cannot set file access advice for file '" << filename << "' (" << strerror(errno) << ")";
      }
    }
    else if (mode==Sequential) {
      if (posix_fadvise(fileno(file),0,size,POSIX_FADV_SEQUENTIAL)<0) {
        log.Error() << "Cannot set file access advice for file '" << filename << "' (" << strerror(errno) << ")";
      }
    }
    else if (mode==LowMemRandom) {
      if (posix_fadvise(fileno(file),0,size,POSIX_FADV_RANDOM)<0) {
        log.Error() << "Cannot set file access advice for file '" << filename << "' (" << strerror(errno) << ")";
      }
    }
#endif

#if defined(HAVE_MMAP)
    if (file!=NULL && useMmap && this->size>0) {
      FreeBuffer();

      buffer=(char*)mmap(NULL,size,PROT_READ,MAP_PRIVATE,fileno(file),0);
      if (buffer!=MAP_FAILED) {
        offset=0;
#if defined(HAVE_POSIX_MADVISE)
        if (mode==FastRandom) {
          if (posix_madvise(buffer,size,POSIX_MADV_WILLNEED)<0) {
            log.Error() << "Cannot set mmaped file access advice for file '" << filename << "' (" << strerror(errno) << ")";
          }
        }
        else if (mode==Sequential) {
          if (posix_madvise(buffer,size,POSIX_MADV_SEQUENTIAL)<0) {
            log.Error() << "Cannot set mmaped file access advice for file '" << filename << "' (" << strerror(errno) << ")";
          }
        }
        else if (mode==LowMemRandom) {
          if (posix_madvise(buffer,size,POSIX_MADV_RANDOM)<0) {
            log.Error() << "Cannot set mmaped file access advice for file '" << filename << "' (" << strerror(errno) << ")";
          }
        }
#endif
      }
      else {
        log.Error() << "Cannot mmap file '" << filename << "' of size " << size << " (" << strerror(errno) << ")";
        buffer=NULL;
      }
    }
#elif  defined(__WIN32__) || defined(WIN32)
    if (file!=NULL && useMmap && this->size>0) {
      FreeBuffer();

      mmfHandle=CreateFileMapping((HANDLE)_get_osfhandle(fileno(file)),
                                  (LPSECURITY_ATTRIBUTES)NULL,
                                  PAGE_READONLY,
                                  0,0,
                                  (LPCTSTR)NULL);

      if (mmfHandle!=NULL) {
        buffer=(char*)MapViewOfFile(mmfHandle,
                                    FILE_MAP_READ,
                                    0,
                                    0,
                                    0);

        if (buffer!=NULL) {
          offset=0;
        }
        else {
          log.Error() << "Cannot map view for file '" << filename << "' of size " << size << " (" << GetLastError() << ")";
        }
      }
      else {
        log.Error() << "Cannot create file mapping for file '" << filename << "' of size " << size << " (" << GetLastError() << ")";
      }
    }
#endif

    hasError=file==NULL;

    return !hasError;
  }

  bool FileScanner::Close()
  {
    bool result;

    if (file==NULL) {
      log.Error() << "File '" << filename << "' already closed, cannot close it again!";
      return false;
    }

    FreeBuffer();

    result=fclose(file)==0;

    file=NULL;

    if (!result) {
      log.Error() << "Cannot close file '" << filename << "' (" << strerror(errno) << ")";
    }

    return result;
  }

  bool FileScanner::IsEOF() const
  {
    if (HasError()) {
      return true;
    }

    if (size==0) {
      return true;
    }

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      return offset>=size;
    }
#endif

    return feof(file)!=0;
  }

  std::string FileScanner::GetFilename() const
  {
    return filename;
  }

  bool FileScanner::GotoBegin()
  {
    return SetPos(0);
  }

  bool FileScanner::SetPos(FileOffset pos)
  {
    if (HasError()) {
      return false;
    }

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (pos>=size) {
        return false;
      }

      offset=pos;

      return true;
    }
#endif

    clearerr(file);

#if defined(HAVE_FSEEKO)
    hasError=fseeko(file,(off_t)pos,SEEK_SET)!=0;
#else
    hasError=fseek(file,pos,SEEK_SET)!=0;
#endif

    if (hasError) {
      log.Error() << "Cannot set file pos for file '" << filename << "' (" << strerror(errno) << ")";
    }

    return !hasError;
  }

  bool FileScanner::GetPos(FileOffset& pos) const
  {
    if (HasError()) {
      return false;
    }

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      pos=offset;
      return true;
    }
#endif

#if defined(HAVE_FSEEKO)
    off_t filepos=ftello(file);

    if (filepos==-1) {
      hasError=true;
    }
    else {
      pos=(FileOffset)filepos;
    }
#else
    long filepos=ftell(file);

    if (filepos==-1) {
      hasError=true;
    }
    else {
      pos=(FileOffset)filepos;
    }
#endif

    if (hasError) {
      log.Error() << "Cannot read file pos for file '" << filename << "' (" << strerror(errno) << ")";
    }

    return !hasError;
  }

  bool FileScanner::Read(char* buffer, size_t bytes)
  {
#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (this->buffer!=NULL) {
      if (offset+(FileOffset)bytes-1>=size) {
        log.Error() << "Cannot read byte array beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      memcpy(buffer,&this->buffer[offset],bytes);

      offset+=bytes;

      return true;
    }
#endif

    hasError=fread(buffer,1,bytes,file)!=bytes;

    if (hasError) {
      log.Error() << "Cannot read byte array from file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    return true;
  }

  bool FileScanner::Read(std::string& value)
  {
    if (HasError()) {
      return false;
    }

    value.clear();

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset>=size) {
        log.Error() << "Cannot read std::string beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      size_t start=offset;

      while (offset<size &&
             buffer[offset]!='\0') {
        offset++;
      }

      value.assign(&buffer[start],offset-start);

      if (offset>=size) {
        log.Error() << "String has no terminating '\\0' before end of file '" << filename << "' (" << strerror(errno) << ")";
        hasError=true;
        return false;
      }

      offset++;

      return true;
    }
#endif

    char character;

    hasError=fread(&character,1,1,file)!=1;

    if (hasError) {
      log.Error() << "Cannot read std::string beyond end of file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    while (character!='\0') {
      value.append(1,character);

      hasError=fread(&character,1,1,file)!=1;

      if (hasError) {
        log.Error() << "String has no terminating '\\0' before end of file '" << filename << "' (" << strerror(errno) << ")";
        return false;
      }
    }

    return true;
  }

  bool FileScanner::Read(bool& boolean)
  {
    if (HasError()) {
      return false;
    }

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset>=size) {
        log.Error() << "Cannot read bool beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      boolean=buffer[offset]!=0;

      offset++;

      return true;
    }
#endif

    char value;

    hasError=fread(&value,1,1,file)!=1;

    if (hasError) {
      log.Error() << "Cannot read bool from file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    boolean=value!=0;

    return true;
  }

  bool FileScanner::Read(int8_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset>=size) {
        log.Error() << "Cannot read int8_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      number=(int8_t)buffer[offset];

      offset++;

      return true;
    }
#endif

    hasError=fread(&number,1,1,file)!=1;

    if (hasError) {
      log.Error() << "Cannot read int8_t from file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    return true;
  }

  bool FileScanner::Read(int16_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+2-1>=size) {
        log.Error() << "Cannot read int16_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char    *dataPtr=&buffer[offset];
      int16_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;

      offset+=2;

      return true;
    }
#endif

    unsigned char buffer[2];

    hasError=fread(&buffer,1,2,file)!=2;

    if (hasError) {
      log.Error() << "Cannot read int16_t from file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    int16_t       add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 8;
    number|=add;

    return true;
  }

  bool FileScanner::Read(int32_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+4-1>=size) {
        log.Error() << "Cannot read int32_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char    *dataPtr=&buffer[offset];
      int32_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 16;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 24;
      number|=add;

      offset+=4;

      return true;
    }
#endif

    unsigned char buffer[4];

    hasError=fread(&buffer,1,4,file)!=4;

    if (hasError) {
      log.Error() << "Cannot read int32_t from file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    int32_t       add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 8;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 16;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 24;
    number|=add;

    return true;
  }

  bool FileScanner::Read(int64_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+8-1>=size) {
        log.Error() << "Cannot read int64_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char    *dataPtr=&buffer[offset];
      int64_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 16;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 24;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 32;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 40;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 48;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 56;
      number|=add;

      offset+=8;

      return true;
    }
#endif

    unsigned char buffer[8];

    hasError=fread(&buffer,1,8,file)!=8;

    if (hasError) {
      log.Error() << "Cannot read int64_t from file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    int64_t       add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 8;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 16;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 24;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 32;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 40;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 48;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 56;
    number|=add;

    return true;
  }

  bool FileScanner::Read(uint8_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset>=size) {
        log.Error() << "Cannot read uint8_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      number=(uint8_t)buffer[offset];

      offset++;

      return true;
    }
#endif

    hasError=fread(&number,1,1,file)!=1;

    if (hasError) {
      log.Error() << "Cannot read uint8_t from file '" << filename << "' (" << strerror(errno) << ")";
      return false;
    }

    return true;
  }

  bool FileScanner::Read(uint16_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+2-1>=size) {
        log.Error() << "Cannot read uint16_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char     *dataPtr=&buffer[offset];
      uint16_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;

      offset+=2;

      return true;
    }
#endif

    unsigned char buffer[2];

    hasError=fread(&buffer,1,2,file)!=2;

    if (hasError) {
      log.Error() << "Cannot read uint16_t from file '" << filename<< "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    uint16_t      add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 8;
    number|=add;

    return true;
  }

  bool FileScanner::Read(uint32_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+4-1>=size) {
        log.Error() << "Cannot read uint32_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char     *dataPtr=&buffer[offset];
      uint32_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 16;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 24;
      number|=add;

      offset+=4;

      return true;
    }
#endif

    unsigned char buffer[4];

    hasError=fread(&buffer,1,4,file)!=4;

    if (hasError) {
      log.Error() << "Cannot read uint32_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    uint32_t      add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 8;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 16;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 24;
    number|=add;

    return true;
  }

  bool FileScanner::Read(uint64_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+8-1>=size) {
        log.Error() << "Cannot read uint64_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char     *dataPtr=&buffer[offset];
      uint64_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 16;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 24;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 32;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 40;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 48;
      number|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 56;
      number|=add;

      offset+=8;

      return true;
    }
#endif

    unsigned char buffer[8];

    hasError=fread(&buffer,1,8,file)!=8;

    if (hasError) {
      log.Error() << "Cannot read uint64_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    uint64_t      add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 8;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 16;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 24;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 32;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 40;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 48;
    number|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 56;
    number|=add;

    return true;
  }

  bool FileScanner::Read(uint16_t& number,
                         size_t bytes)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+bytes-1>=size) {
        log.Error() << "Cannot read uint16_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char     *dataPtr=&buffer[offset];
      uint16_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      if (bytes>=2) {
        add=(unsigned char)(*dataPtr);
        add=add << 8;
        number|=add;
      }

      offset+=bytes;

      return true;
    }
#endif

    unsigned char buffer[2];

    hasError=fread(&buffer,1,bytes,file)!=bytes;

    if (hasError) {
      log.Error() << "Cannot read uint16_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    uint16_t      add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    if (bytes>=2) {
      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;
    }

    return true;
  }

  bool FileScanner::Read(uint32_t& number,
                         size_t bytes)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+bytes-1>=size) {
        log.Error() << "Cannot read uint32_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char     *dataPtr=&buffer[offset];
      uint32_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      if (bytes>=2) {
        add=(unsigned char)(*dataPtr);
        add=add << 8;
        number|=add;
        dataPtr++;

        if (bytes>=3) {
          add=(unsigned char)(*dataPtr);
          add=add << 16;
          number|=add;
          dataPtr++;

          if (bytes>=4) {
            add=(unsigned char)(*dataPtr);
            add=add << 24;
            number|=add;
          }
        }
      }

      offset+=bytes;

      return true;
    }
#endif

    unsigned char buffer[4];

    hasError=fread(&buffer,1,bytes,file)!=bytes;

    if (hasError) {
      log.Error() << "Cannot read uint32_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    uint32_t      add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    if (bytes>=2) {
      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;
      dataPtr++;

      if (bytes>=3) {
        add=(unsigned char)(*dataPtr);
        add=add << 16;
        number|=add;
        dataPtr++;

        if (bytes>=4) {
          add=(unsigned char)(*dataPtr);
          add=add << 24;
          number|=add;
        }
      }
    }

    return true;
  }

  bool FileScanner::Read(uint64_t& number,
                         size_t bytes)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+bytes-1>=size) {
        log.Error() << "Cannot read uint64_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char     *dataPtr=&buffer[offset];
      uint64_t add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      number|=add;
      dataPtr++;

      if (bytes>=2) {
        add=(unsigned char)(*dataPtr);
        add=add << 8;
        number|=add;
        dataPtr++;

        if (bytes>=3) {
          add=(unsigned char)(*dataPtr);
          add=add << 16;
          number|=add;
          dataPtr++;

          if (bytes>=4) {
            add=(unsigned char)(*dataPtr);
            add=add << 24;
            number|=add;
            dataPtr++;

            if (bytes>=5) {
              add=(unsigned char)(*dataPtr);
              add=add << 32;
              number|=add;
              dataPtr++;

              if (bytes>=6) {
                add=(unsigned char)(*dataPtr);
                add=add << 40;
                number|=add;
                dataPtr++;

                if (bytes>=7) {
                  add=(unsigned char)(*dataPtr);
                  add=add << 48;
                  number|=add;
                  dataPtr++;

                  if (bytes>=8) {
                    add=(unsigned char)(*dataPtr);
                    add=add << 56;
                    number|=add;
                  }
                }
              }
            }
          }
        }
      }

      offset+=bytes;

      return true;
    }
#endif

    unsigned char buffer[8];

    hasError=fread(&buffer,1,bytes,file)!=bytes;

    if (hasError) {
      log.Error() << "Cannot read uint64_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    uint64_t      add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    number|=add;
    dataPtr++;

    if (bytes>=2) {
      add=(unsigned char)(*dataPtr);
      add=add << 8;
      number|=add;
      dataPtr++;

      if (bytes>=3) {
        add=(unsigned char)(*dataPtr);
        add=add << 16;
        number|=add;
        dataPtr++;

        if (bytes>=4) {
          add=(unsigned char)(*dataPtr);
          add=add << 24;
          number|=add;
          dataPtr++;

          if (bytes>=5) {
            add=(unsigned char)(*dataPtr);
            add=add << 32;
            number|=add;
            dataPtr++;

            if (bytes>=6) {
              add=(unsigned char)(*dataPtr);
              add=add << 40;
              number|=add;
              dataPtr++;

              if (bytes>=7) {
                add=(unsigned char)(*dataPtr);
                add=add << 48;
                number|=add;
                dataPtr++;

                if (bytes>=8) {
                  add=(unsigned char)(*dataPtr);
                  add=add << 56;
                  number|=add;
                }
              }
            }
          }
        }
      }
    }

    return true;
  }

  bool FileScanner::Read(ObjectFileRef& ref)
  {
    uint8_t    typeByte;
    FileOffset fileOffset;

    if (!(Read(typeByte) &&
          ReadFileOffset(fileOffset))) {
      return false;
    }

    ref.Set(fileOffset,(RefType)typeByte);

    return true;
  }

  bool FileScanner::ReadFileOffset(FileOffset& fileOffset)
  {
    if (HasError()) {
      return false;
    }

    fileOffset=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+8-1>=size) {
        log.Error() << "Cannot read osmscout::FileOffset beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char       *dataPtr=&buffer[offset];
      FileOffset add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      fileOffset|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 8;
      fileOffset|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 16;
      fileOffset|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 24;
      fileOffset|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 32;
      fileOffset|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 40;
      fileOffset|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 48;
      fileOffset|=add;
      dataPtr++;

      add=(unsigned char)(*dataPtr);
      add=add << 56;
      fileOffset|=add;

      offset+=8;

      return true;
    }
#endif

    unsigned char buffer[8];

    hasError=fread(&buffer,1,8,file)!=8;

    if (hasError) {
      log.Error() << "Cannot read osmscout::FileOffset from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    FileOffset    add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    fileOffset|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 8;
    fileOffset|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 16;
    fileOffset|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 24;
    fileOffset|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 32;
    fileOffset|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 40;
    fileOffset|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 48;
    fileOffset|=add;
    dataPtr++;

    add=(unsigned char)(*dataPtr);
    add=add << 56;
    fileOffset|=add;

    return true;
  }

  bool FileScanner::ReadFileOffset(FileOffset& fileOffset,
                                   size_t bytes)
  {
    if (HasError()) {
      return false;
    }

    assert(bytes>0 && bytes<=8);

    fileOffset=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+bytes-1>=size) {
        log.Error() << "Cannot read osmscout::FileOffset beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char       *dataPtr=&buffer[offset];
      FileOffset add;

      add=(unsigned char)(*dataPtr);
      add=add << 0;
      fileOffset|=add;
      dataPtr++;

      if (bytes>=2) {
        add=(unsigned char)(*dataPtr);
        add=add << 8;
        fileOffset|=add;
        dataPtr++;

        if (bytes>=3) {
          add=(unsigned char)(*dataPtr);
          add=add << 16;
          fileOffset|=add;
          dataPtr++;

          if (bytes>=4) {
            add=(unsigned char)(*dataPtr);
            add=add << 24;
            fileOffset|=add;
            dataPtr++;

            if (bytes>=5) {
              add=(unsigned char)(*dataPtr);
              add=add << 32;
              fileOffset|=add;
              dataPtr++;

              if (bytes>=6) {
                add=(unsigned char)(*dataPtr);
                add=add << 40;
                fileOffset|=add;
                dataPtr++;

                if (bytes>=7) {
                  add=(unsigned char)(*dataPtr);
                  add=add << 48;
                  fileOffset|=add;
                  dataPtr++;

                  if (bytes>=8) {
                    add=(unsigned char)(*dataPtr);
                    add=add << 56;
                    fileOffset|=add;
                  }
                }
              }
            }
          }
        }
      }

      offset+=bytes;

      return true;
    }
#endif

    unsigned char buffer[8];

    hasError=fread(&buffer,1,bytes,file)!=bytes;

    if (hasError) {
      log.Error() << "Cannot read osmscout::FileOffset from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    unsigned char *dataPtr=buffer;
    FileOffset    add;

    add=(unsigned char)(*dataPtr);
    add=add << 0;
    fileOffset|=add;
    dataPtr++;

    if (bytes>=2) {
      add=(unsigned char)(*dataPtr);
      add=add << 8;
      fileOffset|=add;
      dataPtr++;

      if (bytes>=3) {
        add=(unsigned char)(*dataPtr);
        add=add << 16;
        fileOffset|=add;
        dataPtr++;

        if (bytes>=4) {
          add=(unsigned char)(*dataPtr);
          add=add << 24;
          fileOffset|=add;
          dataPtr++;

          if (bytes>=5) {
            add=(unsigned char)(*dataPtr);
            add=add << 32;
            fileOffset|=add;
            dataPtr++;

            if (bytes>=6) {
              add=(unsigned char)(*dataPtr);
              add=add << 40;
              fileOffset|=add;
              dataPtr++;

              if (bytes>=7) {
                add=(unsigned char)(*dataPtr);
                add=add << 48;
                fileOffset|=add;
                dataPtr++;

                if (bytes>=8) {
                  add=(unsigned char)(*dataPtr);
                  add=add << 56;
                  fileOffset|=add;
                }
              }
            }
          }
        }
      }
    }

    return true;
  }

  bool FileScanner::ReadNumber(int16_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset>=size) {
        log.Error() << "Cannot read compressed int16_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      unsigned int bytes=DecodeNumber(&buffer[offset],number);

      offset+=bytes;

      return true;
    }
#endif

    char buffer;

    if (fread(&buffer,1,1,file)!=1) {
      log.Error() << "Cannot read compressed int16_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    typedef int16_t num_t;

    unsigned int shift=0;
    unsigned int nextShift=0;

    // negative form
    if ((buffer & 0x01)!=0) {
      char val=(buffer & 0x7e) >> 1;

      number=-1;
      nextShift=6;

      while ((buffer & 0x80)!=0) {

        if (fread(&buffer,1,1,file)!=1) {
          log.Error() << "Cannot read compressed int16_t from file '" << filename <<  "' (" << strerror(errno) << ")";
          hasError=true;
          return false;
        }

        number^=(val << shift);
        val=buffer & 0x7f;
        shift=nextShift;
        nextShift+=7;
      }

      number^=static_cast<num_t>(val) << shift;
    }
    else {
      char val=(buffer & 0x7e) >> 1;

      number=0;
      nextShift=6;

      while ((buffer & 0x80)!=0) {

        if (fread(&buffer,1,1,file)!=1) {
          log.Error() << "Cannot read compressed int16_t from file '" << filename <<  "' (" << strerror(errno) << ")";
          hasError=true;
          return false;
        }

        number|=(val << shift);
        val=buffer & 0x7f;
        shift=nextShift;
        nextShift+=7;
      }

      number|=static_cast<num_t>(val) << shift;
    }

    return true;
  }

  bool FileScanner::ReadNumber(int32_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset>=size) {
        log.Error() << "Cannot read compressed int32_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      unsigned int bytes=DecodeNumber(&buffer[offset],number);

      offset+=bytes;

      return true;
    }
#endif

    char buffer;

    if (fread(&buffer,1,1,file)!=1) {
      log.Error() << "Cannot read compressed int32_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }


    typedef int32_t num_t;

    unsigned int shift=0;
    unsigned int nextShift=0;

    // negative form
    if ((buffer & 0x01)!=0) {
      char val=(buffer & 0x7e) >> 1;

      number=-1;
      nextShift=6;

      while ((buffer & 0x80)!=0) {

        if (fread(&buffer,1,1,file)!=1) {
          log.Error() << "Cannot read compressed int32_t from file '" << filename <<  "' (" << strerror(errno) << ")";
          hasError=true;
          return false;
        }

        number^=(val << shift);
        val=buffer & 0x7f;
        shift=nextShift;
        nextShift+=7;
      }

      number^=static_cast<num_t>(val) << shift;
    }
    else {
      char val=(buffer & 0x7e) >> 1;

      number=0;
      nextShift=6;

      while ((buffer & 0x80)!=0) {

        if (fread(&buffer,1,1,file)!=1) {
          log.Error() << "Cannot read compressed int32_t from file '" << filename <<  "' (" << strerror(errno) << ")";
          hasError=true;
          return false;
        }

        number|=(val << shift);
        val=buffer & 0x7f;
        shift=nextShift;
        nextShift+=7;
      }

      number|=static_cast<num_t>(val) << shift;
    }

    return true;
  }

  bool FileScanner::ReadNumber(int64_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset>=size) {
        log.Error() << "Cannot read compressed int64_t beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      unsigned int bytes=DecodeNumber(&buffer[offset],number);

      offset+=bytes;

      return true;
    }
#endif

    char buffer;

    if (fread(&buffer,1,1,file)!=1) {
      log.Error() << "Cannot read compressed int64_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    typedef int64_t num_t;

    unsigned int shift=0;
    unsigned int nextShift=0;

    // negative form
    if ((buffer & 0x01)!=0) {
      char val=(buffer & 0x7e) >> 1;

      number=-1;
      nextShift=6;

      while ((buffer & 0x80)!=0) {

        if (fread(&buffer,1,1,file)!=1) {
          log.Error() << "Cannot read compressed int64_t from file '" << filename <<  "' (" << strerror(errno) << ")";
          hasError=true;
          return false;
        }

        number^=(val << shift);
        val=buffer & 0x7f;
        shift=nextShift;
        nextShift+=7;
      }

      number^=static_cast<num_t>(val) << shift;
    }
    else {
      char val=(buffer & 0x7e) >> 1;

      number=0;
      nextShift=6;

      while ((buffer & 0x80)!=0) {

        if (fread(&buffer,1,1,file)!=1) {
          log.Error() << "Cannot read compressed int64_t from file '" << filename <<  "' (" << strerror(errno) << ")";
          hasError=true;
          return false;
        }

        number|=(val << shift);
        val=buffer & 0x7f;
        shift=nextShift;
        nextShift+=7;
      }

      number|=static_cast<num_t>(val) << shift;
    }

    return true;
  }

  bool FileScanner::ReadNumber(uint16_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      unsigned int shift=0;

      for (; offset<size; offset++) {
        number|=(buffer[offset] & 127) << shift;

        if ((buffer[offset] & 128)==0) {
          offset++;
          return true;
        }

        shift+=7;
      }

      log.Error() << "Cannot read compressed uint16_t beyond end of file '" << filename << "'";
      hasError=true;
      return false;
    }
#endif

    char buffer;

    if (fread(&buffer,1,1,file)!=1) {
      log.Error() << "Cannot read compressed uint16_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    unsigned int shift=0;

    while (true) {
      number|=static_cast<uint16_t>(buffer & '\x7f') << shift;

      if ((buffer & '\x80')==0) {
        return true;
      }

      if (fread(&buffer,1,1,file)!=1) {
        log.Error() << "Cannot read compressed uint16_t from file '" << filename <<  "' (" << strerror(errno) << ")";
        hasError=true;
        return false;
      }

      shift+=7;
    }

    return true;
  }

  bool FileScanner::ReadNumber(uint32_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      unsigned int shift=0;

      for (; offset<size; offset++) {
        number|=(buffer[offset] & 127) << shift;

        if ((buffer[offset] & 128)==0) {
          offset++;
          return true;
        }

        shift+=7;
      }

      log.Error() << "Cannot read compressed uint32_t beyond end of file '" << filename << "'";
      hasError=true;
      return false;
    }
#endif

    char buffer;

    if (fread(&buffer,1,1,file)!=1) {
      log.Error() << "Cannot read compressed uint32_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    unsigned int shift=0;

    while (true) {
      number|=static_cast<uint32_t>(buffer & '\x7f') << shift;

      if ((buffer & '\x80')==0) {
        return true;
      }

      if (fread(&buffer,1,1,file)!=1) {
        log.Error() << "Cannot read compressed uint32_t from file '" << filename <<  "' (" << strerror(errno) << ")";
        hasError=true;
        return false;
      }

      shift+=7;
    }

    return true;
  }

  bool FileScanner::ReadNumber(uint64_t& number)
  {
    if (HasError()) {
      return false;
    }

    number=0;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      unsigned int shift=0;

      for (; offset<size; offset++) {
        number|=(buffer[offset] & 127) << shift;

        if ((buffer[offset] & 128)==0) {
          offset++;
          return true;
        }

        shift+=7;
      }

      log.Error() << "Cannot read compressed uint64_t beyond end of file '" << filename << "'";
      hasError=true;
      return false;
    }
#endif

    char buffer;

    if (fread(&buffer,1,1,file)!=1) {
      log.Error() << "Cannot read compressed uint64_t from file '" << filename <<  "' (" << strerror(errno) << ")";
      hasError=true;
      return false;
    }

    unsigned int shift=0;

    while (true) {
      number|=static_cast<uint64_t>(buffer & '\x7f') << shift;

      if ((buffer & '\x80')==0) {
        return true;
      }

      if (fread(&buffer,1,1,file)!=1) {
        log.Error() << "Cannot read compressed uint64_t from file '" << filename <<  "' (" << strerror(errno) << ")";
        hasError=true;
        return false;
      }

      shift+=7;
    }

    return true;
  }

  bool FileScanner::ReadCoord(GeoCoord& coord)
  {
    if (HasError()) {
      return false;
    }

    uint32_t latDat;
    uint32_t lonDat;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+coordByteSize-1>=size) {
        log.Error() << "Cannot read osmscout::GeoCoord beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char *dataPtr=&buffer[offset];

      latDat=  ((unsigned char) dataPtr[0] <<  0)
             | ((unsigned char) dataPtr[1] <<  8)
             | ((unsigned char) dataPtr[2] << 16)
             | ((unsigned char) (dataPtr[6] & 0x0f) << 24);

      lonDat=  ((unsigned char) dataPtr[3] <<  0)
             | ((unsigned char) dataPtr[4] <<  8)
             | ((unsigned char) dataPtr[5] << 16)
             | ((unsigned char) (dataPtr[6] & 0xf0) << 20);

      offset+=coordByteSize;

      coord.Set(latDat/latConversionFactor-90.0,
                lonDat/lonConversionFactor-180.0);

      return true;
    }
#endif

    unsigned char buffer[coordByteSize];

    hasError=fread(&buffer,1,coordByteSize,file)!=coordByteSize;

    if (hasError) {
      log.Error() << "Cannot read osmscout::GeoCoord from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    latDat=  (buffer[0] <<  0)
           | (buffer[1] <<  8)
           | (buffer[2] << 16)
           | ((buffer[6] & 0x0f) << 24);

    lonDat=  (buffer[3] <<  0)
           | (buffer[4] <<  8)
           | (buffer[5] << 16)
           | ((buffer[6] & 0xf0) << 20);

    coord.Set(latDat/latConversionFactor-90.0,
              lonDat/lonConversionFactor-180.0);

    return true;
  }

  bool FileScanner::ReadConditionalCoord(GeoCoord& coord,
                                         bool& isSet)
  {
    if (HasError()) {
      return false;
    }

    uint32_t latDat;
    uint32_t lonDat;

#if defined(HAVE_MMAP) || defined(__WIN32__) || defined(WIN32)
    if (buffer!=NULL) {
      if (offset+coordByteSize-1>=size) {
        log.Error() << "Cannot read osmscout::GeoCoord beyond end of file '" << filename << "'";
        hasError=true;
        return false;
      }

      char *dataPtr=&buffer[offset];

      latDat=  ((unsigned char) dataPtr[0] <<  0)
             | ((unsigned char) dataPtr[1] <<  8)
             | ((unsigned char) dataPtr[2] << 16)
             | ((unsigned char) (dataPtr[6] & 0x0f) << 24);

      lonDat=  ((unsigned char) dataPtr[3] <<  0)
             | ((unsigned char) dataPtr[4] <<  8)
             | ((unsigned char) dataPtr[5] << 16)
             | ((unsigned char) (dataPtr[6] & 0xf0) << 20);

      offset+=coordByteSize;

      if (latDat==0xfffffff &&
          lonDat==0xfffffff) {
        isSet=false;
      }
      else  {
        coord.Set(latDat/latConversionFactor-90.0,
                  lonDat/lonConversionFactor-180.0);
        isSet=true;
      }

      return true;
    }
#endif

    unsigned char buffer[coordByteSize];

    hasError=fread(&buffer,1,coordByteSize,file)!=coordByteSize;

    if (hasError) {
      log.Error() << "Cannot read osmscout::GeoCoord from file '" << filename <<  "' (" << strerror(errno) << ")";
      return false;
    }

    latDat=  (buffer[0] <<  0)
           | (buffer[1] <<  8)
           | (buffer[2] << 16)
           | ((buffer[6] & 0x0f) << 24);

    lonDat=  (buffer[3] <<  0)
           | (buffer[4] <<  8)
           | (buffer[5] << 16)
           | ((buffer[6] & 0xf0) << 20);

    if (latDat==0xfffffff &&
        lonDat==0xfffffff) {
      isSet=false;
    }
    else  {
      coord.Set(latDat/latConversionFactor-90.0,
                lonDat/lonConversionFactor-180.0);
      isSet=true;
    }

    return true;
  }

  bool FileScanner::Read(std::vector<GeoCoord>& nodes)
  {
    size_t  coordBitSize;
    uint8_t sizeByte;

    if (!Read(sizeByte)) {
      return false;
    }

    // Fast exit for empty arrays
    if (sizeByte==0) {
      return true;
    }

    if ((sizeByte & 0x03) == 0) {
      coordBitSize=16;
    }
    else if ((sizeByte & 0x03) == 1) {
      coordBitSize=32;
    }
    else {
      coordBitSize=48;
    }

    size_t nodeCount=(sizeByte & 0x7c) >> 2;

    if ((sizeByte & 0x80) != 0) {
      if (!Read(sizeByte)) {
        return false;
      }

      nodeCount|=(sizeByte & 0x7f) << 5;

      if ((sizeByte & 0x80) != 0) {
        if (!Read(sizeByte)) {
          return false;
        }

        nodeCount|=sizeByte << 12;
      }
    }

    //std::cout << "Read " << std::dec << nodeCount << " nodes, " << coordBitSize << " bits per coordinate pair" << std::endl;

    nodes.resize(nodeCount);

    size_t byteBufferSize=(nodeCount-1)*coordBitSize/8;

    AssureByteBufferSize(byteBufferSize);

    if (!ReadCoord(nodes[0])) {
      return false;
    }

    uint32_t latValue=(uint32_t)round((nodes[0].GetLat()+90.0)*latConversionFactor);
    uint32_t lonValue=(uint32_t)round((nodes[0].GetLon()+180.0)*lonConversionFactor);

    if (!Read((char*)byteBuffer,byteBufferSize)) {
      return false;
    }

    /* std::cout << "Read - byte buffer: ";
    for (size_t i=0; i<byteBufferSize; i++) {
      std::cout << std::hex << (unsigned int) byteBuffer[i] << " ";
    }
    std::cout << std::endl;*/

    //std::cout << "Read - calculated deltas: ";
    if (coordBitSize==16) {
      size_t currentCoordPos=1;

      for (size_t i=0; i<byteBufferSize; i+=2) {
        int32_t latDelta=(int8_t)byteBuffer[i];
        int32_t lonDelta=(int8_t)byteBuffer[i+1];

        latValue+=latDelta;
        lonValue+=lonDelta;

        //std::cout << std::hex << latDelta << " " << lonDelta << " ";

        nodes[currentCoordPos].Set(latValue/latConversionFactor-90.0,
                                   lonValue/lonConversionFactor-180.0);

        currentCoordPos++;
      }
      //std::cout << std::endl;
    }
    else if (coordBitSize==32) {
      size_t currentCoordPos=1;

      for (size_t i=0; i<byteBufferSize; i+=4) {
        uint32_t latUDelta=byteBuffer[i+0] | (byteBuffer[i+1]<<8);
        uint32_t lonUDelta=byteBuffer[i+2] | (byteBuffer[i+3]<<8);
        int32_t  latDelta;
        int32_t  lonDelta;

        if (latUDelta & 0x8000) {
          latDelta=(int32_t)(latUDelta | 0xffff0000);
        }
        else {
          latDelta=(int32_t)latUDelta;
        }

        latValue+=latDelta;

        if (lonUDelta & 0x8000) {
          lonDelta=(int32_t)(lonUDelta | 0xffff0000);
        }
        else {
          lonDelta=(int32_t)lonUDelta;
        }

        lonValue+=lonDelta;

        //std::cout << std::hex << latDelta << " " << lonDelta << " ";

        nodes[currentCoordPos].Set(latValue/latConversionFactor-90.0,
                                   lonValue/lonConversionFactor-180.0);
        currentCoordPos++;
      }
      //std::cout << std::endl;
    }
    else {
      size_t currentCoordPos=1;

      for (size_t i=0; i<byteBufferSize; i+=6) {
        uint32_t latUDelta=(byteBuffer[i+0]) | (byteBuffer[i+1]<<8) | (byteBuffer[i+2]<<16);
        uint32_t lonUDelta=(byteBuffer[i+3]) | (byteBuffer[i+4]<<8) | (byteBuffer[i+5]<<16);
        int32_t  latDelta;
        int32_t  lonDelta;

        if (latUDelta & 0x800000) {
          latDelta=(int32_t)(latUDelta | 0xff000000);
        }
        else {
          latDelta=(int32_t)latUDelta;
        }

        latValue+=latDelta;

        if (lonUDelta & 0x800000) {
          lonDelta=(int32_t)(lonUDelta | 0xff000000);
        }
        else {
          lonDelta=(int32_t)lonUDelta;
        }

        lonValue+=lonDelta;

        nodes[currentCoordPos].Set(latValue/latConversionFactor-90.0,
                                   lonValue/lonConversionFactor-180.0);

        currentCoordPos++;
      }
    }

    return !HasError();
  }

  bool FileScanner::ReadBox(GeoBox& box)
  {
    if (HasError()) {
      return false;
    }

    GeoCoord minCoord;
    GeoCoord maxCoord;

    if (!(ReadCoord(minCoord) &&
          ReadCoord(maxCoord))) {
      return false;
    }

    box.Set(minCoord,
            maxCoord);

    return !HasError();
  }

  bool FileScanner::ReadTypeId(TypeId& id,
                               uint8_t maxBytes)
  {
    if (maxBytes==1) {
      uint8_t byteValue;

      if (Read(byteValue)) {
        id=byteValue;

        return true;
      }
      else {
        return false;
      }
    }
    else {
      return ReadNumber(id);
    }
  }

  ObjectFileRefStreamReader::ObjectFileRefStreamReader(FileScanner& reader)
  : reader(reader),
    lastFileOffset(0)
  {
    // no code
  }

  void ObjectFileRefStreamReader::Reset()
  {
    lastFileOffset=0;
  }

  bool ObjectFileRefStreamReader::Read(ObjectFileRef& ref)
  {
    RefType    type;
    FileOffset offset;

    if (!reader.ReadNumber(offset)) {
      return false;
    }

    type=(RefType)(offset%4);

    offset=offset >> 2;
    offset=offset+lastFileOffset;

    ref.Set(offset,type);

    lastFileOffset=offset;

    return true;
  }
}
