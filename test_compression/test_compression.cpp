#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <assert.h>
#include <conio.h>

#include <windows.h>

#pragma warning(disable:4996)
#pragma comment(lib, "winmm.lib" )
#pragma comment(lib, "wsock32.lib" )
#pragma comment(lib, "Ws2_32.lib" )

#include "../compression/compression.h"


#define MAXNUMERIC 32  // JWR  support up to 16 32 character long numeric formated strings
#define MAXFNUM    16

static	char  gFormat[MAXNUMERIC*MAXFNUM];
static int    gIndex=0;

const char * formatNumber(int number) // JWR  format this integer into a fancy comma delimited string
{
	char * dest = &gFormat[gIndex*MAXNUMERIC];
	gIndex++;
	if ( gIndex == MAXFNUM ) gIndex = 0;

	char scratch[512];

#if defined (LINUX_GENERIC)
	snprintf(scratch, 10, "%d", number);
#else
	itoa(number,scratch,10);
#endif

	char *str = dest;
	unsigned int len = (unsigned int)strlen(scratch);
	for (unsigned int i=0; i<len; i++)
	{
		int place = (len-1)-i;
		*str++ = scratch[i];
		if ( place && (place%3) == 0 ) *str++ = ',';
	}
	*str = 0;

	return dest;
}

void * testCompress(const void *mem,int len,COMPRESSION::CompressionType type,int &outlen)
{
  const char *tag = COMPRESSION::getCompressionTypeString(type);
  printf("Compress:%-10s :", tag );
  unsigned int stime = timeGetTime();
  void *cdata = COMPRESSION::compressData(mem,len,outlen,type);
  unsigned int etime = timeGetTime();
  printf("FROM:%11s TO:%11s %4d%% %8s MS\r\n", formatNumber(len), formatNumber(outlen), (outlen*100) / len, formatNumber(etime-stime) );
  return cdata;
}

void testDecompress(void *cdata,int clen)
{
  if ( cdata && clen )
  {
    COMPRESSION::CompressionType type = COMPRESSION::getCompressionType(cdata,clen);
    if ( type == COMPRESSION::CT_INVALID )
    {
      printf("Decompress:%-10s\r\n", "CT_INVALID");
    }
    else
    {
      const char *tag = COMPRESSION::getCompressionTypeString(type);
      printf("Decompress:%-10s :", tag );
      unsigned int stime = timeGetTime();
      int outlen;
      void *udata = COMPRESSION::decompressData(cdata,clen,outlen);
      unsigned etime = timeGetTime();
      printf("FROM:%11s TO:%11s %8s MS\r\n", formatNumber(clen), formatNumber(outlen), formatNumber(etime-stime) );
      COMPRESSION::deleteData(udata);
    }
    COMPRESSION::deleteData(cdata);
  }
}


void main(int argc,const char **argv)
{

  char dirname[512];
  strncpy(dirname,argv[0],512);
  int len = strlen(dirname);
  char *scan = &dirname[len-1];
  while ( len )
  {
    if ( *scan == '\\' )
    {
      *scan = 0;
      break;
    }
    scan--;
    len--;
  }

  chdir( dirname );

  // make the current working directory be the directory the executable launched from.
  const char *fname = "test_file.xml";

  if ( argc == 2 )
  {
    fname = argv[1];
  }

  FILE *fph = fopen(fname,"rb");
  if ( fph )
  {
    fseek(fph,0L,SEEK_END);
    int len = ftell(fph);
    fseek(fph,0L,SEEK_SET);
    if ( len > 0 )
    {
      printf("Reading test file '%s' which is %s bytes long.\r\n", fname, formatNumber((int)len) );
      printf("\r\n");
      printf("---------------------------------------------------------------\r\n");
      printf("Testing Compression rate and speed with various compressors.\r\n");
      printf("---------------------------------------------------------------\r\n");
      char *temp = new char[len];
      fread(temp, len, 1, fph );


#if USE_CRYPTO
      int outlen2;
      void *cdata2 = testCompress(temp,len,COMPRESSION::CT_CRYPTO_GZIP,outlen2);
#endif
#if USE_MINI_LZO
      int outlen3;
      void * cdata3 = testCompress(temp,len,COMPRESSION::CT_MINILZO,outlen3);
#endif
      int outlen4;
      void * cdata4 = testCompress(temp,len,COMPRESSION::CT_ZLIB,outlen4);

      int outlen5;
      void * cdata5 = testCompress(temp,len,COMPRESSION::CT_BZIP,outlen5);

      int outlen6;
      void * cdata6 = testCompress(temp,len,COMPRESSION::CT_LIBLZF,outlen6);

      delete []temp;

      printf("\r\n");
      printf("---------------------------------------------------------------\r\n");
      printf("Testing Decompression speed with various decompressors.\r\n");
      printf("---------------------------------------------------------------\r\n");
#if USE_CRYPTO
      testDecompress(cdata2,outlen2);
#endif
#if USE_MINI_LZO
      testDecompress(cdata3,outlen3);
#endif
      testDecompress(cdata4,outlen4);
      testDecompress(cdata5,outlen5);
      testDecompress(cdata6,outlen6);


    }
    else
    {
      printf("Can't compress file '%s', it is an emtpy file.\r\n");
    }
    fclose(fph);
  }
  else
  {
    printf("Failed to open test file '%s' for input.\r\n", fname );
  }

  printf("Press a key to exit.\r\n");
  getch();
}

