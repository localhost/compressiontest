#ifndef COMPRESSION_H

#define COMPRESSION_H


#define USE_MINI_LZO 1 // Since MINILZO is GPL, this implementation is disabled by default.
#define USE_CRYPTO 1   // The source to the CyrptoPP library is not included in this download as it is too large.  If you have access to the source,add it to your include path and set this #define to 1
#define USE_ZLIB 1

namespace COMPRESSION
{

enum CompressionType
{
  CT_INVALID,
  CT_CRYPTO_GZIP,       // The CryptoPP library implementation of GZIP   http://www.cryptopp.com
  CT_MINILZO,           // The MiniLZO library  http://www.oberhumer.com/opensource/lzo/
  CT_ZLIB,              // The ZLIB library   http://www.zlib.net/
  CT_BZIP,              // The BZIP library  http://www.bzip.org/
  CT_LIBLZF,            // The LIBLZF library  http://oldhome.schmorp.de/marc/liblzf.html
};

void *           compressData(const void *source,int len,int &outlen,CompressionType type=CT_ZLIB);
void *           decompressData(const void *source,int clen,int &outlen);
void             deleteData(void* mem);

CompressionType  getCompressionType(const void *mem,int len);
const char      *getCompressionTypeString(CompressionType type);

};


#endif
