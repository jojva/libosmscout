#ifndef OSMSCOUT_FILESCANNER_H
#define OSMSCOUT_FILESCANNER_H

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

#include <cstdio>
#include <string>
#include <vector>

#include <osmscout/private/CoreImportExport.h>

#include <osmscout/CoreFeatures.h>

#include <osmscout/GeoCoord.h>
#include <osmscout/ObjectRef.h>
#include <osmscout/Types.h>

#include <osmscout/util/GeoBox.h>

#if defined(__WIN32__) || defined(WIN32)
  #include <windows.h>
  #undef max
  #undef min
#endif

namespace osmscout {

  /**
    \ingroup File

    FileScanner implements platform independent sequential
    scanning-like access to data in files. File access is buffered.

    FileScanner will use mmap in read-only mode if available (and will
    fall back to normal buffered IO if available but failing), resulting in
    mapping the complete file into the memory of the process (without
    allocating real memory) resulting in measurable speed increase because of
    exchanging buffered file access with in memory array access.
    */
  class OSMSCOUT_API FileScanner
  {
  public:
    enum Mode
    {
      Sequential,
      FastRandom,
      LowMemRandom,
      Normal
    };

  private:
    std::string          filename;       //!< Filename
    std::FILE            *file;          //!< Internal low level file handle
    mutable bool         hasError;       //!< Flag to signal errors in the stream

    // For mmap usage
    char                 *buffer;        //!< Pointer to the file memory
    FileOffset           size;           //!< Size of the memory/file
    FileOffset           offset;         //!< Current offset into the file memory

    // For std::vector<GeoCoord> loading
    uint8_t              *byteBuffer;    //!< Temporary buffer for loading of std::vector<GeoCoord>
    size_t               byteBufferSize; //!< Size of the temporary byte buffer

    // For Windows mmap usage
#if defined(__WIN32__) || defined(WIN32)
    HANDLE       mmfHandle;
#endif

  private:
    void AssureByteBufferSize(size_t size);
    void FreeBuffer();

  public:
    FileScanner();
    virtual ~FileScanner();

    bool Open(const std::string& filename,
              Mode mode,
              bool useMmap);
    bool Close();

    inline bool IsOpen() const
    {
      return file!=NULL;
    }

    bool IsEOF() const;

    inline  bool HasError() const
    {
      return file==NULL || hasError;
    }

    std::string GetFilename() const;

    bool GotoBegin();
    bool SetPos(FileOffset pos);
    bool GetPos(FileOffset &pos) const;

    bool Read(char* buffer, size_t bytes);

    bool Read(std::string& value);

    bool Read(bool& boolean);

    bool Read(int8_t& number);
    bool Read(int16_t& number);
    bool Read(int32_t& number);
    bool Read(int64_t& number);

    bool Read(uint8_t& number);
    bool Read(uint16_t& number);
    bool Read(uint32_t& number);
    bool Read(uint64_t& number);

    bool Read(uint16_t& number, size_t bytes);
    bool Read(uint32_t& number, size_t bytes);
    bool Read(uint64_t& number, size_t bytes);

    bool Read(ObjectFileRef& ref);

    bool ReadFileOffset(FileOffset& offset);
    bool ReadFileOffset(FileOffset& offset,
                        size_t bytes);

    bool ReadNumber(int16_t& number);
    bool ReadNumber(int32_t& number);
    bool ReadNumber(int64_t& number);

    bool ReadNumber(uint16_t& number);
    bool ReadNumber(uint32_t& number);
    bool ReadNumber(uint64_t& number);

    bool ReadCoord(GeoCoord& coord);
    bool ReadConditionalCoord(GeoCoord& coord,
                              bool& isSet);

    bool Read(std::vector<GeoCoord>& nodes);

    bool ReadBox(GeoBox& box);

    bool ReadTypeId(TypeId& id,
                    uint8_t maxBytes);
  };

  /**
   * Read back a stream of sorted ObjectFileRefs as written by the ObjectFileRefStreamWriter.
   */
  class OSMSCOUT_API ObjectFileRefStreamReader
  {
  private:
    FileScanner& reader;
    FileOffset   lastFileOffset;

  public:
    ObjectFileRefStreamReader(FileScanner& reader);

    void Reset();

    bool Read(ObjectFileRef& ref);
  };

}

#endif
