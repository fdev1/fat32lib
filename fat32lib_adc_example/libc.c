#include <stdio.h>
#include <p30sim.h>
#include <errno.h>
#include <string.h>
#include "..\smlib\sm.h"

#define O_RDONLY	0x0000
#define O_WRONLY	0x0001
#define O_RDWR		0x0002
#define O_CREAT		0x0100	
#define O_EXCL		0x0200
#define O_TRUNC		0x0800
#define O_APPEND	0x1000

#define LIBC __attribute__((__section__(".libc")))

extern int __C30_UART;
extern volatile UxMODEBITS U2MODEbits __attribute__((__sfr__,weak));
extern volatile UxSTABITS U2STAbits __attribute__((__sfr__,weak));
extern volatile unsigned int U2TXREG __attribute__((__sfr__,weak));
extern volatile unsigned int U2BRG __attribute__((__sfr__,weak));

static SM_FILE files[3];


int LIBC open(const char *name, int access, int mode) {

	int ret;
	int file_id = 0;
	unsigned char access_flags = 0;
	SM_FILE* f = 0;

	if (access & O_RDONLY) access_flags |= SM_FILE_ACCESS_READ;
	if (access & O_WRONLY) access_flags |= SM_FILE_ACCESS_WRITE;
	if (access & O_RDWR) access_flags |= SM_FILE_ACCESS_READ | SM_FILE_ACCESS_WRITE;
	if (access & O_APPEND) access_flags |= SM_FILE_ACCESS_APPEND;
	if (access & O_TRUNC) access_flags |= SM_FILE_ACCESS_OVERWRITE;
	if (access & O_CREAT) access_flags |= SM_FILE_ACCESS_CREATE;
	if (access & (O_WRONLY | O_RDWR) &&
		!(access & (O_TRUNC | O_APPEND))) access_flags |= SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE;

	// todo: implement O_EXCL

	for (ret = 0; ret < 3; ret++)
	{
		if (files[ret].magic == 0)
		{
			f = &files[ret];
			file_id = ret + 3;
			break;
		}
	}

	if (!f)
	{
		errno = SM_INSUFFICIENT_MEMORY;
		return -1;
	}

	ret = sm_file_open(f, (char*) name, access_flags);

	if (ret != SM_SUCCESS)
	{
		errno = ret;
		return -1;
	}

	return (int) file_id;
}


int LIBC close(int handle) 
{
	if (handle >= 3)
	{
		sm_file_close(&files[handle - 3]);
		files[handle - 3].magic = 0;
	}
	return 0;
}

long LIBC lseek(int handle, long offset, int origin) 
{
	char mode;
	int ret;

	switch (origin)
	{
		case SEEK_SET: mode = SM_SEEK_START; break;
		case SEEK_CUR: mode = SM_SEEK_CURRENT; break;
		case SEEK_END: mode = SM_SEEK_END; break;
	}

	ret = sm_file_seek(&files[handle - 3], offset, mode);
	if (ret != SM_SUCCESS)
	{
		errno = ret;
		return -1;
	}

 	return offset;
}

int LIBC read(int handle, void *buffer, unsigned int len)
{
  	int i;
	volatile UxMODEBITS *umode = &U1MODEbits;
  	volatile UxSTABITS *ustatus = &U1STAbits;
  	volatile unsigned int *rxreg = &U1RXREG;
  	volatile unsigned int *brg = &U1BRG;


  	switch (handle)
  	{
    	case 0:
			#ifdef __C30_LEGACY_LIBC__
      		if (_Files[0]->_Lockno == 0)
			#endif
      		{
        		if ((__C30_UART != 1) && (&U2BRG)) 
				{
          			umode = &U2MODEbits;
          			ustatus = &U2STAbits;
          			rxreg = &U2RXREG;
          			brg = &U2BRG;
        		}
        		if ((umode->UARTEN) == 0)
        		{
          			*brg = 0;
          			umode->UARTEN = 1;
        		}
        		for (i = len; i; --i)
        		{
          			int nTimeout;

          			/*
          			** Timeout is 16 cycles per 10-bit char
          			*/
          			nTimeout = 16*10;
          			while (((ustatus->URXDA) == 0) && nTimeout) --nTimeout;
          			if ((ustatus->URXDA) == 0) break;
          			*(char*)buffer++ = *rxreg;
        		}
        		len -= i;
        		break;
      		}
		case 1:
		case 2:
			break;

    	default: 
		{	
			int ret;
			ret = sm_file_write(&files[handle - 3], (unsigned char*) buffer, len);
			if (ret != SM_SUCCESS)
			{
				errno = ret;
				return 0;
			}
    	}
  	}
  	return(len);
}

int LIBC remove(const char *filename) 
{
	return sm_file_delete((char*)filename);
}

int LIBC rename(const char *oldname, const char *newname) 
{
	return sm_file_rename((char*)oldname, (char*)newname);
}

#define __C30_UART 	2

int LIBC write(int handle, void *buffer, unsigned int len) 
{
  	int i;
  	volatile UxMODEBITS *umode = &U1MODEbits;
  	volatile UxSTABITS *ustatus = &U1STAbits;
  	volatile unsigned int *txreg = &U1TXREG;
  	volatile unsigned int *brg = &U1BRG;

  	switch (handle)
  	{
    	case 0:
    	case 1: 
    	case 2:
      		if ((__C30_UART != 1) && (&U2BRG)) 
			{
        		umode = &U2MODEbits;
        		ustatus = &U2STAbits;
        		txreg = &U2TXREG;
        		brg = &U2BRG;
      		}
      		if ((umode->UARTEN) == 0)
      		{
        		*brg = 0;
        		umode->UARTEN = 1;
      		}
      		if ((ustatus->UTXEN) == 0)
      		{
        		ustatus->UTXEN = 1;
      		}
      		for (i = len; i; --i)
      		{
        		while ((ustatus->TRMT) ==0);
        		*txreg = *(char*)buffer++;
      		}
      		break;

    	default: 
		{
      		i = sm_file_write(&files[handle - 3], (unsigned char*) buffer, (unsigned long) len);
			if (i != SM_SUCCESS)
			{
				errno = i;
				return 0;
			}
	      	break;
    	}
  	}
  	return(len);
}
