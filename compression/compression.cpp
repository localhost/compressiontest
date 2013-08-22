#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "compression.h"

#if USE_MINI_LZO
#include "../minilzo/minilzo.h"
#endif

#if USE_CRYPTO
#include "../cryptopp/cryptlib.h"
#include "../cryptopp/gzip.h"
#endif

#if USE_ZLIB
#include "../zlib/zlib.h"
#endif

#include "../bzip/bzlib.h"

extern "C"
{

#include "../liblzf/lzf.h"

#include "../lzma/LzmaEnc.h"
#include "../lzma/LzmaDec.h"

}

#include "../fastlz/fastlz.h"

#include "../miniz/miniz.h"

namespace COMPRESSION
{

typedef unsigned long CRC32; // crc datatype.
const CRC32 CRC32_POLYNOMIAL=0xEDB88320L;

static  bool         mFirst=true;
static  unsigned int CRCTable[256];

static void BuildCRCTable(void)
{
  int i;
  int j;
  unsigned int crc;

  for ( i = 0; i <= 255 ; i++ )
  {
    crc = i;
    for ( j = 8 ; j > 0; j-- )
    {
      if ( crc & 1 )
        crc = ( crc >> 1 ) ^ CRC32_POLYNOMIAL;
      else
        crc >>= 1;
    }
    CRCTable[i] = crc;
  }
}

static inline unsigned int ComputeCRC(char c,unsigned int &crc)  // add into accumulated crc.
{
  unsigned int temp1 = ( crc >> 8 ) & 0x00FFFFFFL;
  unsigned int temp2 = CRCTable[ ( (int) crc ^ c ) & 0xff ];
  crc = temp1 ^ temp2;
  return crc;
}

unsigned int ComputeCRC(const void *buffer,int count,unsigned int crc=0)
{

  if ( mFirst )
  {
    BuildCRCTable();
    mFirst = false;
  }

  crc = crc^count;
  const unsigned char * p = (const unsigned char*) buffer;
  while ( count-- != 0 )
  {
    ComputeCRC( *p++, crc );
  }
  return( crc&0x7FFFFFFF );
}

void deleteData(void* mem)
{
  free(mem);
}

struct CompressionHeader
{
  int             mRawLength;
  int             mCompressedLength;
  unsigned int    mCRC;
  char            mId[4];
};

void * compressMiniLZO(const void *source,int len,int &outlen)
{
#if USE_MINI_LZO
  const int SCRATCHPAD=65536;
  char wrkmem[LZO1X_1_MEM_COMPRESS];

  CompressionHeader *h = (CompressionHeader *) malloc(len+SCRATCHPAD+sizeof(CompressionHeader));
  unsigned char *dest = (unsigned char *)h;
  dest+=sizeof(CompressionHeader);
  lzo_init();
  int r = lzo1x_1_compress((const unsigned char *)source,len,dest,(lzo_uint *)&outlen,wrkmem);

  if ( r == LZO_E_OK )
  {
    h->mRawLength        = len;
    h->mCRC              = ComputeCRC((const unsigned char *)dest,outlen,h->mRawLength);
    outlen+=sizeof(CompressionHeader);
    h->mCompressedLength = outlen;
    h->mId[0]            = 'M';
    h->mId[1]            = 'L';
    h->mId[2]            = 'Z';
    h->mId[3]            = 'O';

  }
  else
  {
    free(h);
    h = 0;
  }

  return h;
#else

  outlen = 0;
  return 0;

#endif

}

#if USE_CRYPTO
void * compressCRYPTO_GZIP(const void *source,int len,int &outlen)
{
  void * ret = 0;

  outlen = 0;
  std::string outStringOfBytes;
  CryptoPP::Gzip zipper( new CryptoPP::StringSink(outStringOfBytes)); // I have been told that this is not a memory leak, that zipper takes ownership of this memory.  Which is bullshit, but what are you gonna do?
  zipper.Put( (const byte *)source,len);
  zipper.MessageEnd();

  unsigned int slen = outStringOfBytes.length();
  const char *data  = outStringOfBytes.c_str();

  outlen = slen+sizeof(CompressionHeader);
  char *dest = (char *)malloc(outlen);
  ret = dest;

  CompressionHeader *h = (CompressionHeader *) dest;

  h->mRawLength        = len;
  h->mCompressedLength = outlen;
  h->mCRC              = ComputeCRC(data,slen,h->mRawLength);
  h->mId[0]            = 'C';
  h->mId[1]            = 'R';
  h->mId[2]            = 'P';
  h->mId[3]            = 'T';

  h++;
  dest = (char *) h;
  memcpy(dest,data,slen);

  return ret;
}
#endif

#if USE_ZLIB
void * compressZLIB(const void *source,int len,int &outlen)
{

  uLong csize = compressBound(len);

  CompressionHeader *h = (CompressionHeader *) malloc(csize+sizeof(CompressionHeader));
  unsigned char *dest = (unsigned char *)h;
  dest+=sizeof(CompressionHeader);

  int err = compress2((Bytef *)dest,&csize,(Bytef *)source,len, Z_BEST_SPEED);

  if ( err == Z_OK )
  {
    outlen = csize;

    h->mRawLength        = len;
    h->mCRC              = ComputeCRC((const unsigned char *)dest,outlen,h->mRawLength);
    outlen+=sizeof(CompressionHeader);
    h->mCompressedLength = outlen;
    h->mId[0]            = 'Z';
    h->mId[1]            = 'L';
    h->mId[2]            = 'I';
    h->mId[3]            = 'B';
  }
  else
  {
    outlen = 0;
    free(h);
    h = 0;
  }

  return h;
}
#endif

void * compressBZIP(const void *source,int len,int &outlen)
{

  unsigned int csize = len+65536;

  CompressionHeader *h = (CompressionHeader *) malloc(csize+sizeof(CompressionHeader));
  unsigned char *dest = (unsigned char *)h;
  dest+=sizeof(CompressionHeader);

  int err = BZ2_bzBuffToBuffCompress((char *)dest,&csize,(char *)source,(unsigned int)len,1,0,30);

  if ( err == 0 )
  {
    outlen = csize;

    h->mRawLength        = len;
    h->mCRC              = ComputeCRC((const unsigned char *)dest,outlen,h->mRawLength);
    outlen+=sizeof(CompressionHeader);
    h->mCompressedLength = outlen;
    h->mId[0]            = 'B';
    h->mId[1]            = 'Z';
    h->mId[2]            = 'I';
    h->mId[3]            = 'P';
  }
  else
  {
    outlen = 0;
    free(h);
    h = 0;
  }

  return h;
}

void * compressLIBLZF(const void *source,int len,int &outlen)
{
  unsigned int csize = len + 65536;
  CompressionHeader *h = (CompressionHeader *) malloc(csize+sizeof(CompressionHeader));
  unsigned char *dest = (unsigned char *)h;
  dest+=sizeof(CompressionHeader);

  outlen = lzf_compress(source, len, dest, csize);

  if (outlen != 0)
  {
    h->mRawLength        = len;
    h->mCRC              = ComputeCRC((const unsigned char *)dest,outlen,h->mRawLength);
    outlen+=sizeof(CompressionHeader);
    h->mCompressedLength = outlen;
    h->mId[0]            = 'L';
    h->mId[1]            = 'L';
    h->mId[2]            = 'Z';
    h->mId[3]            = 'F';
  }
  else
  {
    free(h);
	h = 0;
  }

  return h;
}

void* Alloc(void *p, size_t size) { return malloc(size); }
void Free(void *p, void *address) { if (address) free(address); }

ISzAlloc alloc = { Alloc, Free };

void * compressLZMA(const void *source,int len,int &outlen)
{

  uLong csize = len*2;

  CompressionHeader *h = (CompressionHeader *) malloc(csize+sizeof(CompressionHeader));
  unsigned char *dest = (unsigned char *)h;
  dest+=sizeof(CompressionHeader);

    CLzmaEncProps props;
    LzmaEncProps_Init(&props);
    props.level = 1;
    props.algo = 0;
    props.numThreads = 4;
    SizeT s = LZMA_PROPS_SIZE;
    SRes err = LzmaEncode((Byte*)dest + LZMA_PROPS_SIZE, (SizeT*)&csize, (Byte*)source, len, &props, (Byte*)dest, &s, 1, NULL, &alloc, &alloc);
    csize += LZMA_PROPS_SIZE;

  if ( err == SZ_OK )
  {
    outlen = csize;

    h->mRawLength        = len;
    h->mCRC              = ComputeCRC((const unsigned char *)dest,outlen,h->mRawLength);
    outlen+=sizeof(CompressionHeader);
    h->mCompressedLength = outlen;
    h->mId[0]            = 'L';
    h->mId[1]            = 'Z';
    h->mId[2]            = 'M';
    h->mId[3]            = 'A';
  }
  else
  {
    outlen = 0;
    free(h);
    h = 0;
  }
  return h;
}

void * compressFASTLZ(const void *source,int len,int &outlen)
{
  unsigned int csize = len*2;
  CompressionHeader *h = (CompressionHeader *) malloc(csize+sizeof(CompressionHeader));
  unsigned char *dest = (unsigned char *)h;
  dest+=sizeof(CompressionHeader);

  outlen = fastlz_compress(source, len, dest);

  if (outlen != 0)
  {
    h->mRawLength        = len;
    h->mCRC              = ComputeCRC((const unsigned char *)dest,outlen,h->mRawLength);
    outlen+=sizeof(CompressionHeader);
    h->mCompressedLength = outlen;
    h->mId[0]            = 'F';
    h->mId[1]            = 'A';
    h->mId[2]            = 'S';
    h->mId[3]            = 'T';
  }
  else
  {
    free(h);
	h = 0;
  }

  return h;
}

void * compressMINIZ(const void *source,int len,int &outlen)
{
    unsigned int csize = compressBound(len);
    CompressionHeader *h = (CompressionHeader *) malloc(csize+sizeof(CompressionHeader));
    unsigned char *dest = (unsigned char *)h;
    dest+=sizeof(CompressionHeader);

    uLong out_len = csize;
    int result = compress(dest, &out_len, (const unsigned char *)source, len);
    outlen = out_len;

    if (outlen != 0 && result == 0)
    {
        h->mRawLength        = len;
        h->mCRC              = ComputeCRC((const unsigned char *)dest,outlen,h->mRawLength);
        outlen+=sizeof(CompressionHeader);
        h->mCompressedLength = outlen;
        h->mId[0]            = 'M';
        h->mId[1]            = 'I';
        h->mId[2]            = 'N';
        h->mId[3]            = 'I';
    }
    else
    {
        free(h);
        h = 0;
    }

    return h;
}

void * compressData(const void *source,int len,int &outlen,CompressionType type)
{
  void *ret = 0;

  switch ( type )
  {
#if USE_CRYPTO
    case CT_CRYPTO_GZIP:
      ret = compressCRYPTO_GZIP(source,len,outlen);
      break;
#endif
    case CT_MINILZO:
      ret = compressMiniLZO(source,len,outlen);
      break;
    case CT_ZLIB:
#if USE_ZLIB
      ret = compressZLIB(source,len,outlen);
#endif
      break;
    case CT_BZIP:
      ret = compressBZIP(source,len,outlen);
      break;
    case CT_LIBLZF:
      ret = compressLIBLZF(source,len,outlen);
      break;
    case CT_LZMA:
      ret = compressLZMA(source,len,outlen);
      break;
    case CT_FASTLZ:
      ret = compressFASTLZ(source,len,outlen);
      break;
    case CT_MINIZ:
      ret = compressMINIZ(source,len,outlen);
      break;

  }
  return ret;
}

void * decompressMiniLZO(const void *source,int clen,int &outlen)
{
#if USE_MINI_LZO

  void * ret = 0;

  CompressionHeader *h = (CompressionHeader *) source;

  if ( 1 )
  {
    const char *data = (const char *) h;
    data+=sizeof(CompressionHeader);
    unsigned int slen = clen-sizeof(CompressionHeader);
    unsigned int crc = ComputeCRC((const unsigned char *)data,slen,h->mRawLength);
    if ( crc == h->mCRC )
    {
      outlen = h->mRawLength;
      char *dest = (char *)malloc(h->mRawLength);
      lzo_init();

      int r = lzo1x_decompress((const unsigned char *)data,
                               slen,
                               (unsigned char *)dest,
                               (lzo_uint *)&outlen,0);

      if ( r == LZO_E_OK )
      {
        ret = dest;
        assert( outlen == h->mRawLength );
        outlen = h->mRawLength;
      }
      else
      {
        free(dest);
        ret = 0;
      }
    }
  }

  return ret;
#else
  outlen = 0;
  return 0;
#endif
}

#if USE_CRYPTO
void * decompressCRYPTO_GZIP(const void *source,int clen,int &outlen)
{
  void * ret = 0;

  outlen = 0;

  CompressionHeader *h = (CompressionHeader *) source;
  if ( 1 )
  {
    const char *data = (const char *) h;
    data+=sizeof(CompressionHeader);
    unsigned int slen = clen-sizeof(CompressionHeader);

    unsigned int crc = ComputeCRC(data,slen,h->mRawLength);

    if ( crc == h->mCRC )
    {
      CryptoPP::Gunzip zipper;
      zipper.PutMessageEnd((const byte *)data,slen);
      char *dest = (char *)malloc(h->mRawLength);
      zipper.Get((byte *)dest,h->mRawLength);
      ret = dest;
      outlen = h->mRawLength;
    }
  }

  return ret;
}
#endif

#if USE_ZLIB
void * decompressZLIB(const void *source,int clen,int &outlen)
{
  void * ret = 0;

  CompressionHeader *h = (CompressionHeader *) source;

  if ( 1 )
  {

    const char *data = (const char *) h;
    data+=sizeof(CompressionHeader);
    unsigned int slen = clen-sizeof(CompressionHeader);
    unsigned int crc = ComputeCRC((const unsigned char *)data,slen,h->mRawLength);
    if ( crc == h->mCRC )
    {

      outlen = h->mRawLength;
      char *dest = (char *)malloc(h->mRawLength);

      int err;

      uLongf destLen = outlen;

      err = uncompress( (Bytef *) dest,&destLen,(Bytef *) data, slen );

      assert( destLen == outlen );
      if ( destLen != outlen )
        err = -1;

      if ( err == Z_OK )
      {
        ret = dest;
      }
      else
      {
        free(dest);
        outlen = 0;
        ret = 0;
      }
    }
  }

  return ret;
}
#endif

void * decompressBZIP(const void *source,int clen,int &outlen)
{
  void * ret = 0;

  CompressionHeader *h = (CompressionHeader *) source;

  if ( 1 )
  {

    const char *data = (const char *) h;
    data+=sizeof(CompressionHeader);
    unsigned int slen = clen-sizeof(CompressionHeader);
    unsigned int crc = ComputeCRC((const unsigned char *)data,slen,h->mRawLength);
    if ( crc == h->mCRC )
    {

      outlen = h->mRawLength;
      char *dest = (char *)malloc(h->mRawLength);

      int err;

      unsigned int destLen = outlen;

      err = BZ2_bzBuffToBuffDecompress( (char *)dest, &destLen, (char *)data, slen, 0, 0);

      assert( destLen == outlen );
      if ( destLen != outlen )
        err = -1;

      if ( err == 0 )
      {
        ret = dest;
      }
      else
      {
        free(dest);
        outlen = 0;
        ret = 0;
      }
    }
  }

  return ret;
}

void * decompressLIBLZF(const void *source,int clen,int &outlen)
{
  void * ret = 0;

  CompressionHeader *h = (CompressionHeader *) source;

  if ( 1 )
  {

    const char *data = (const char *) h;
    data+=sizeof(CompressionHeader);
    unsigned int slen = clen-sizeof(CompressionHeader);
    unsigned int crc = ComputeCRC((const unsigned char *)data,slen,h->mRawLength);
    if ( crc == h->mCRC )
    {

      outlen = h->mRawLength;
      char *dest = (char *)malloc(h->mRawLength);

      unsigned int destLen = outlen;
      outlen = lzf_decompress(data, slen, dest, h->mRawLength);

      assert( destLen == outlen );

	  int err = 0;
	  if ( destLen != outlen )
        err = -1;

      if ( err == 0 )
      {
        ret = dest;
      }
      else
      {
        free(dest);
        outlen = 0;
        ret = 0;
      }
    }
  }

  return ret;
}

void * decompressLZMA(const void *source,int clen,int &outlen)
{
  void * ret = 0;

  CompressionHeader *h = (CompressionHeader *) source;

  if ( 1 )
  {

    const char *data = (const char *) h;
    data+=sizeof(CompressionHeader);
    unsigned int slen = clen-sizeof(CompressionHeader);
    unsigned int crc = ComputeCRC((const unsigned char *)data,slen,h->mRawLength);
    if ( crc == h->mCRC )
    {

      outlen = h->mRawLength;
      char *dest = (char *)malloc(h->mRawLength);

      unsigned int destLen = outlen;

      ELzmaStatus status;
      slen -= LZMA_PROPS_SIZE;
      SRes err = LzmaDecode((Byte*)dest, (SizeT*)&destLen, (const Byte*)data + LZMA_PROPS_SIZE, (SizeT*)&slen, (const Byte*)data, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &alloc);

      assert( destLen == outlen );

      if ( err == SZ_OK && destLen == outlen )
      {
        ret = dest;
      }
      else
      {
        free(dest);
        outlen = 0;
        ret = 0;
      }
    }
  }

  return ret;
}

void * decompressFASTLZ(const void *source,int clen,int &outlen)
{
  void * ret = 0;

  CompressionHeader *h = (CompressionHeader *) source;

  if ( 1 )
  {

    const char *data = (const char *) h;
    data+=sizeof(CompressionHeader);
    unsigned int slen = clen-sizeof(CompressionHeader);
    unsigned int crc = ComputeCRC((const unsigned char *)data,slen,h->mRawLength);
    if ( crc == h->mCRC )
    {

      outlen = h->mRawLength;
      char *dest = (char *)malloc(h->mRawLength);

      unsigned int destLen = outlen;
      outlen = fastlz_decompress(data, slen, dest, h->mRawLength);

      assert( destLen == outlen );

	  int err = 0;
	  if ( destLen != outlen )
        err = -1;

      if ( err == 0 )
      {
        ret = dest;
      }
      else
      {
        free(dest);
        outlen = 0;
        ret = 0;
      }
    }
  }

  return ret;
}

void * decompressMINIZ(const void *source,int clen,int &outlen)
{
    void * ret = 0;

    CompressionHeader *h = (CompressionHeader *) source;

    if ( 1 )
    {

        const char *data = (const char *) h;
        data+=sizeof(CompressionHeader);
        unsigned int slen = clen-sizeof(CompressionHeader);
        unsigned int crc = ComputeCRC((const unsigned char *)data,slen,h->mRawLength);
        if ( crc == h->mCRC )
        {

            outlen = h->mRawLength;
            char *dest = (char *)malloc(h->mRawLength);

            uLongf destLen = outlen;
            int result = uncompress((Bytef *)dest, &destLen, (const Bytef *)data, slen);

            assert( destLen == outlen );

            int err = 0;
            if ( destLen != outlen )
                err = -1;

            if ( err == 0 )
            {
                ret = dest;
            }
            else
            {
                free(dest);
                outlen = 0;
                ret = 0;
            }
        }
    }

    return ret;
}

void * decompressData(const void *source,int clen,int &outlen)
{
  void * ret = 0;

  outlen = 0;

  switch ( getCompressionType(source,clen) )
  {
#if USE_CRYPTO
    case CT_CRYPTO_GZIP:
      ret = decompressCRYPTO_GZIP(source,clen,outlen);
      break;
#endif
    case CT_MINILZO:
      ret = decompressMiniLZO(source,clen,outlen);
      break;
    case CT_ZLIB:
#if USE_ZLIB
      ret = decompressZLIB(source,clen,outlen);
#endif
      break;
    case CT_BZIP:
      ret = decompressBZIP(source,clen,outlen);
      break;
    case CT_LIBLZF:
      ret = decompressLIBLZF(source,clen,outlen);
      break;
    case CT_LZMA:
      ret = decompressLZMA(source,clen,outlen);
      break;
    case CT_FASTLZ:
      ret = decompressFASTLZ(source,clen,outlen);
      break;
    case CT_MINIZ:
      ret = decompressMINIZ(source,clen,outlen);
      break;
  }

  return ret;
}

CompressionType getCompressionType(const void *mem,int len)
{
  CompressionType ret = CT_INVALID;

  if ( mem && len > sizeof(CompressionHeader) )
  {
    CompressionHeader *h = (CompressionHeader *) mem;
    if ( h->mCompressedLength == len )
    {
      if ( h->mId[0] == 'M' && h->mId[1] == 'L' && h->mId[2] == 'Z' && h->mId[3] == 'O' )
        ret = CT_MINILZO;
      else if ( h->mId[0] == 'C' && h->mId[1] == 'R' && h->mId[2] == 'P' && h->mId[3] == 'T' )
        ret = CT_CRYPTO_GZIP;
      else if ( h->mId[0] == 'Z' && h->mId[1] == 'L' && h->mId[2] == 'I' && h->mId[3] == 'B' )
        ret = CT_ZLIB;
      else if ( h->mId[0] == 'B' && h->mId[1] == 'Z' && h->mId[2] == 'I' && h->mId[3] == 'P' )
        ret = CT_BZIP;
      else if ( h->mId[0] == 'L' && h->mId[1] == 'L' && h->mId[2] == 'Z' && h->mId[3] == 'F' )
        ret = CT_LIBLZF;
      else if ( h->mId[0] == 'L' && h->mId[1] == 'Z' && h->mId[2] == 'M' && h->mId[3] == 'A' )
        ret = CT_LZMA;
      else if ( h->mId[0] == 'F' && h->mId[1] == 'A' && h->mId[2] == 'S' && h->mId[3] == 'T' )
        ret = CT_FASTLZ;
      else if ( h->mId[0] == 'M' && h->mId[1] == 'I' && h->mId[2] == 'N' && h->mId[3] == 'I' )
        ret = CT_MINIZ;
    }
  }

  return ret;
}

const char      *getCompressionTypeString(CompressionType type)
{
  const char *ret = "UNKOWN!??";
  switch ( type )
  {
    case CT_INVALID: ret = "CT_INVALID"; break;
    case CT_CRYPTO_GZIP: ret = "CT_CRYPTO"; break;
    case CT_MINILZO: ret = "CT_MINILZO"; break;
    case CT_ZLIB: ret = "CT_ZLIB"; break;
    case CT_BZIP: ret = "CT_BZIP"; break;
    case CT_LIBLZF: ret = "CT_LIBLZF"; break;
    case CT_LZMA: ret = "CT_LZMA"; break;
    case CT_FASTLZ: ret = "CT_FASTLZ"; break;
    case CT_MINIZ: ret = "CT_MINIZ"; break;
  }
  return ret;
}

}; // end of namespace