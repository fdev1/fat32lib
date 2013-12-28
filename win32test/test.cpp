/*
 * win32test - Win32 Test App for fat32lib and smlib
 * Copyright (C) 2013 Fernando Rodriguez (frodriguez.developer@outlook.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Versioni 3 as 
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <conio.h>
#include <time.h>
#include "..\fat32lib\fat.h"
#include "..\fat32lib\filesystem_interface.h"
#include "..\fat32lib\fat_format.h"
#include "..\smlib\sm.h"
#include "..\win32io\win32io.h"

/*
// LOCAL_FILE -		A file on your local hard drive that will be copied
//					to the mounted filesystem to test file writes
//
// LOCAL_FOLDER -	A local folder where we'll write files that we read from
//					the mounted file system to thest file reads
//
// CHKDSK_PATH -	The full past to chkdsk.exe
//
// SD_CARD -		The drive letter to your SD card drive or any other
//					FAT volume that you'd like to use to test this library.
//					WARNING!!: This volume will be formatted during the tests.
//
*/
#define LOCAL_FILE				"C:\\20861e6dae78f969b1\\mrt.exe"		/*this should be an .exe file*/
#define LOCAL_FOLDER			"C:\\"
#define CHKDSK_PATH				"c:\\windows\\system32\\chkdsk.exe"
#define SD_CARD					"E:"
#define USE_FIXED_POINT			/* to calculate filesize */
#define USE_LONG_FILENAMES

/*
// struct used to store context of async operations
*/
typedef struct CALLBACK_CONTEXT
{
	uint16_t result;
	size_t bytes_read;
	SM_FILE file;
	FILE* f;
	size_t bytes_written;
	time_t start;
	time_t end;
	double ellapsed;
	unsigned char buff[1024];
	char completed;
}
CALLBACK_CONTEXT;

/*
// file scope variables
*/
static FAT_VOLUME fat_volume;
static STORAGE_DEVICE storage_device;

/*
// tests
*/
static char test_format(unsigned char fs_type, STORAGE_DEVICE* stor_device);
static void test_directory_struct();
static void test_write_file(char prealloc);
static void test_read_file();
static void test_append_file(char prealloc);
static void test_create_100_files();
static void test_seek_file();
static void test_delete_file();
static void test_rename_file();
static void test_dir_listing();
static void test_write_file_async();
static void write_file_async_callback(CALLBACK_CONTEXT* context, uint16_t* result);
static char test_chkdsk();
static void format_filesize(uint32_t filesize, char* output);
static void test_read_file_async();
static void read_file_async_callback(CALLBACK_CONTEXT* context, uint16_t* result);
static void test_read_file_unbuffered();
static void test_write_file_unbuffered(char prealloc);
static void test_another_async_write_callback(SM_FILE* file, uint16_t* result);
static void test_another_async_write();
static void test_write_stream();
static void test_write_stream_callback(SM_FILE* file, uint16_t* result, unsigned char** buffer, uint16_t* action);
static void test_check_file(unsigned char* filename);


int cmd_test(char* args)
{
	char buffer[512];

	uint16_t r;
	int i;
	FILESYSTEM fat_filesystem;
	TCHAR* dev = TEXT("\\\\.\\") TEXT(SD_CARD);
	//TCHAR* dev = TEXT("\\\\.\\PhysicalDrive2");

	/*
	
	int x, y;
	int freq;

	for (x = 1; x <= 64; x++)
	{
		for (y = 1; y <= 31; y++)
		{
			//if (((x * 14) + (x * y)) == 2500)
			//{
			freq = 40000000 / ((x * 14) + (x * y));

			if (freq == (freq / 100) * 100)
			{
				printf("ADCS = %i, SAMC = %i, Ksps = %.1f\n", x, y, ((double) freq) / 1000);
			}
			//}
		}
	}

	return;
	*/



	test_check_file("e:\\stream.txt");
	//test_check_file("e:\\xxx1.txt");
	//test_check_file("e:\\xxx2.txt");
	//test_check_file("e:\\xxx3.txt");
	return;

	/*
	// register the FAT filesystem driver with smlib
	*/
	fat_get_filesystem_interface(&fat_filesystem);
	sm_register_filesystem(&fat_filesystem);

	for (r = FAT_FS_TYPE_UNSPECIFIED; r <= FAT_FS_TYPE_UNSPECIFIED; r++)
	{
		printf("Running test on ");
		if (r == FAT_FS_TYPE_FAT12) printf("FAT12 ");
		if (r == FAT_FS_TYPE_FAT16) printf("FAT16 ");
		if (r == FAT_FS_TYPE_FAT32) printf("FAT32 ");
		printf("filesystem...\n");
		/*
		// get the storage device interface
		*/
		win32io_get_storage_device(dev, &storage_device);
		/*
		// format the volume
		*/
		//if (!test_format(r, &storage_device))
		//{
		//	win32io_release_storage_device();
		//	printf("\n\n");
		//	continue;
		//}
		/*
		// mount volume
		*/
		sm_mount_volume("x:", &storage_device); 
		/*sm_dismount_volume("x:");
		sm_mount_volume("x:", &storage_device); */
		/*
		// perform tests
		*/
		/*
		test_directory_struct();
		*/
		test_create_100_files();
		/*
		test_write_file_async();
		test_another_async_write();
		*/
		//test_write_file_unbuffered(1);
		/*
		test_read_file_unbuffered();
		test_write_file(1);
		test_read_file();
		test_read_file_async();
		test_append_file(1);	
		test_seek_file();
		test_rename_file();
		test_write_file(1);
		test_delete_file();	
		*/
		//test_write_stream();
		test_dir_listing();
		
		/*
		// release the lock on the volume
		*/
		sm_dismount_volume("x:");
		win32io_release_storage_device();
		
		printf("\n\n");

		if (!test_chkdsk())
		{
			printf("\nThere where erros. Aborting.\n");
		}
		else 
		{
			printf("\n");
			if (r == FAT_FS_TYPE_FAT12) printf("FAT12 ");
			if (r == FAT_FS_TYPE_FAT16) printf("FAT16 ");
			if (r == FAT_FS_TYPE_FAT32) printf("FAT32 ");
			printf("test completed successfully.\n\n");
		}
	}

	printf("\n");
	return 0;
}

static char test_chkdsk()
{
	DWORD exit_code = 0;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	memset(&si, 0, sizeof(STARTUPINFO));
	/*
	// launch chkdsk process
	*/
	if (!CreateProcess(TEXT(CHKDSK_PATH), 
		TEXT("chkdsk.exe ") TEXT(SD_CARD), NULL, NULL, TRUE, (DWORD)NULL, NULL, NULL, &si, &pi))
	{
		printf("Could not start chkdsk.\n");
		return 0;
	}
	/*
    // wait for process to exit, get exit code
	// and release handles
	*/
    WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
	/*
	// return 1 if chkdsk found no errors, 0 otherwise
	*/
	if (exit_code)
	{
		return 0;
	}
	return 1;
}

static char test_format(unsigned char fs_type, STORAGE_DEVICE* stor_device)
{
	uint16_t r;
	/*
	// print formatting message
	*/
	printf("Formatting ");
	if (fs_type == FAT_FS_TYPE_FAT12) printf("FAT12 ");
	if (fs_type == FAT_FS_TYPE_FAT16) printf("FAT16 ");
	if (fs_type == FAT_FS_TYPE_FAT32) printf("FAT32 ");
	printf("volume...");
	/*
	// format the volume
	*/
	r = fat_format_volume(fs_type, "SD Card", 0, stor_device);
	if (r != FAT_SUCCESS)
	{
		/*
		// print error code and return 0
		*/
		printf("Format error: %x", r);
		return 0;
	}
	/*
	// print completed message and return 1
	*/
	printf("Completed.\n");
	return 1;
}

static void test_delete_file()
{
	uint16_t ret;

	printf("Deleting file...");

	ret = sm_file_delete("x:\\mrt.exe");
	if (ret != FAT_SUCCESS)
	{
		printf("Error deleting file: %x\n", ret);
		return;
	}
	printf("Done.\n");
}

static void test_rename_file()
{
	uint16_t ret;
	printf("Renaming file...");
	#if defined(USE_LONG_FILENAMES)
	ret = sm_file_rename("x:\\mrt.exe", "x:\\mrt_renamed.exe");
	#else
	ret = sm_file_rename("x:\\mrt.exe", "x:\\mrt_ren.exe");
	#endif
	if (ret != FAT_SUCCESS)
	{
		printf("Error: %x\n", ret);
		return;
	}
	printf("Done.\n");
}

static void test_dir_listing()
{
	uint16_t ret;
	int i;
	SM_QUERY q;
	SM_DIRECTORY_ENTRY entry;
	char filename[36];
	char filesize[15];
	char timestamp[20];
	char dirs = 0;
	struct tm * timeinfo;

	filename[35] = 0;

	printf("Directory listing of x:\\...\n\n");

	ret = sm_find_first_entry("x:\\", 0, &entry, &q);
next_dir:

	printf("Filename                            ");
	printf("Size           ");
	printf("Creation Time      \n");
	printf("----------------------------------- ");
	printf("-------------- ");
	printf("------------------ \n");

	while (*entry.name != 0)
	{
		for (i = 0; i < 35; i++)
		{
			if (i < strlen(entry.name))
			{
				filename[i] = entry.name[i];
			}
			else
			{
				filename[i] = 0x20;
			}
		}

		timeinfo = localtime (&entry.create_time);
		strftime(timestamp, 20, "%b %d %H:%M", timeinfo);

		if (entry.attributes & SM_ATTR_DIRECTORY)
		{
			sprintf(filesize, "<DIR>         ", filename, timestamp);
		}
		else
		{
			format_filesize(entry.size, filesize);
		}
		printf("%s %s %s\n", filename, filesize, timestamp);
		ret = sm_find_next_entry(&entry, &q);
	}
	ret = sm_find_close(&q);
	if (!dirs)
	{
#if defined(USE_LONG_FILENAMES)
#define MY_NEW_DIR	"My New Directory"
#else
#define MY_NEW_DIR	"mynewdir"
#endif
		printf("\n\n");
		printf("Directory listing of x:\\" MY_NEW_DIR "...\n\n");
		dirs++;

		ret = sm_find_first_entry("x:\\" MY_NEW_DIR, 0, &entry, &q);
		goto next_dir;
	}
}

static void format_filesize(uint32_t filesize, char* output)
{
	int i;
	int suffix = 0;
	char* suffix_string[] = { "bytes", "KiB", "MiB", "GiB" };

	#if defined(USE_FIXED_POINT)	
	#define unit				(1024LL << 16)
	#define unit_divisor		>> 10
	uint64_t size;
	uint32_t fraction;

	/*
	// add 16 bits after decimal point;
	*/
	size = (uint64_t) filesize << 16;
	#else
	#define unit			1024
	#define unit_divisor	/ 1024
	double size;
	size = (double) filesize;
	#endif


	while (size > unit)
	{
		size = size unit_divisor;
		if (++suffix == 4)
			break;
	}

	if (suffix)
	{
#if defined(USE_FIXED_POINT)
		filesize = (uint32_t) (size >> 16);
		fraction = (((uint32_t) size) << 16) / 0x28F5C28;
		sprintf(output, "%i.%02d %s", filesize, fraction, suffix_string[suffix]);
#else
		sprintf(output, "%.2f", size);
#endif
	}
	else
	{
#if defined(USE_FIXED_POINT)
		filesize = (uint32_t) (size >> 16);
#else
		filesize = (uint32_t) size;
#endif
		sprintf(output, "%i %s", filesize, suffix_string[suffix]);
	}

	for (i = strlen(output); i < 13; i++)
		output[i] = 0x20;
	output[14] = 0;
 
}

static void test_directory_struct()
{
	uint16_t r;
	uint16_t i;
	SM_FILE file;
	char buff[50];
	char hello[] = "Hello World!";
	char goodbye[] = "Goodbye cruel world!";

	printf("Creating files and folders...");

#if defined(USE_LONG_FILENAMES)
#define MY_NEW_DIRECTORY	MY_NEW_DIR
#define HELLO				"Hello World"
#define DIR_NAME			"My Long Filename Folder"
#else
#define MY_NEW_DIRECTORY	MY_NEW_DIR
#define HELLO				"Hello"
#define DIR_NAME			"fldr"
#endif

	r = sm_create_directory("x:\\Test");
	r = sm_create_directory("x:\\Test\\" MY_NEW_DIRECTORY);
	r = sm_create_directory("x:\\" MY_NEW_DIRECTORY);
	r = sm_create_directory("x:\\" DIR_NAME);

	r = sm_file_open(&file, "x:\\" MY_NEW_DIRECTORY "\\" HELLO ".txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\" MY_NEW_DIRECTORY "\\" HELLO "2.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\" MY_NEW_DIRECTORY "\\" HELLO "2.txt", SM_FILE_ACCESS_APPEND);
	r = sm_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\" HELLO ".txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\" HELLO "2.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\" HELLO "2.txt", SM_FILE_ACCESS_APPEND);
	r = sm_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\hey.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\bye.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	r = sm_file_close(&file);

	r = sm_file_open(&file, "x:\\" DIR_NAME "\\hello.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = sm_file_close(&file);

	printf("Completed.\n");

}

static void test_create_100_files()
{
	uint16_t r;
	uint16_t i;
	SM_FILE file;
	char buff1[50];
	char buff2[50];
	time_t start;
	time_t end;
	double ellapsed;

	printf("Creating 100 files...");
	time(&start);

#if defined(USE_LONG_FILENAMES)
#define LOTS_OF_FILES		"Lots of Files"
#define FILE_NAME			"HeLLoooooooo"
#else
#define LOTS_OF_FILES		"Lots"
#define FILE_NAME			"HeLo"
#endif

	//
	// create a directory
	//
	r = sm_create_directory("x:\\" LOTS_OF_FILES);
	if (r != SM_SUCCESS && r != FILESYSTEM_FILENAME_ALREADY_EXISTS)
	{
		printf("Could not create folder '" LOTS_OF_FILES "'. Error: %x\n", r);
	}

	//
	// create 100 files
	//
	for (i = 1; i < 101; i++)
	{
		sprintf(buff1, "x:\\" LOTS_OF_FILES "\\" FILE_NAME "%i.txt", i);
		sprintf(buff2, "Hello, my name is " FILE_NAME "%i.txt", i);

		r = sm_file_open(&file, buff1, FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE);

		if (r == SM_SUCCESS)
		{
			r = sm_file_write(&file, (unsigned char*) buff2, strlen(buff2));

			if (r == SM_SUCCESS)
			{
				r = sm_file_close(&file);

				if (r != SM_SUCCESS)
				{
					printf("Could not close file '%s'. Error: %x\n", buff1, r);
				}
			}
			else
			{
				printf("Could write file '%s'. Error: %x\n", buff1, r);
			}
		}
		else
		{
			printf("Could not open file '%s'. Error: %x\n", buff1, r);
		}
	}
	time(&end);
	ellapsed = difftime(end, start);
	printf("Completed in %.f seconds\n", ellapsed);
}

static void test_check_file(unsigned char* filename)
{
	FILE* f;
	unsigned char buff[4096];
	int i;
	int c = 0;
	size_t bytes_read;
	size_t total_bytes_read = 0;
	int value = 0;

	f = fopen(filename, "rb");
	if (!f)
	{
		printf("Cannot open file.\n");
		return;
	}

	bytes_read = fread(buff, 1, 4096, f);
	total_bytes_read += bytes_read;
	while (bytes_read)
	{
		for (i = 0; i < bytes_read; i += 4)
		{
			if (*((int*) &buff[i]) != value++)
			{
				printf("File corrupted.\n");
				//fclose(f);
				//return;
			}
		}
		c++;
		bytes_read = fread(buff, 1, 4096, f);
		total_bytes_read += bytes_read;
	}
	fclose(f);
	printf("File OK\n");
}

static void test_write_file(char prealloc)
{
	SM_FILE file;
	uint32_t r;
	unsigned char buff[0xFFFF];
	size_t bytes_written = 0;		
	size_t bytes_read;
	size_t sz;
	FILE* f;

	time_t start;
	time_t end;
	double ellapsed;

	printf("Writing file (prealloc:%i)...", prealloc);
	time(&start);

	/*
	// read a file from the local filesystem and write it to the device
	*/
	f = fopen(LOCAL_FILE, "rb");
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);

	r = sm_file_open(&file, "x:\\mrt.exe", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);

	if (prealloc)
	{
		r = sm_file_alloc(&file, sz);
		if (r != FAT_SUCCESS)
		{
			printf("prealloc error: 0x%x", r);
		}
	}

	do
	{
		bytes_read = fread(buff, 1, prealloc ? 1024 : 0xFFFe, f);
		if (bytes_read)
		{
			r = sm_file_write(&file, buff, bytes_read);
			bytes_written += bytes_read;
		}
	}
	while (bytes_read);
	r = sm_file_close(&file);
	fclose(f);

	time(&end);
	ellapsed = difftime(end, start);

	printf("Completed in %.f seconds\n", ellapsed);

}

static void test_write_file_unbuffered(char prealloc)
{
	SM_FILE file;
	uint32_t r;
	unsigned char buff[0xFFFF];
	size_t bytes_written = 0;		
	size_t bytes_read;
	size_t sz;
	FILE* f;

	time_t start;
	time_t end;
	double ellapsed;

	printf("Writing file (prealloc:%i/unbuffered)...", prealloc);
	time(&start);

	/*
	// read a file from the local filesystem and write it to the device
	*/
	f = fopen(LOCAL_FILE, "rb");
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);

	r = sm_file_open(&file, "x:\\mrt_unbf.exe", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE | SM_FILE_FLAG_NO_BUFFERING);

	if (prealloc)
	{
		r = sm_file_alloc(&file, sz);
		if (r != FAT_SUCCESS)
		{
			printf("prealloc error: 0x%x", r);
		}
	}

	do
	{
		bytes_read = fread(buff, 1, prealloc ? 1024 : 0xFFFe, f);
		if (bytes_read)
		{
			r = sm_file_write(&file, buff, 1024);
			bytes_written += bytes_read;
		}
	}
	while (bytes_read);
	r = sm_file_close(&file);
	fclose(f);

	time(&end);
	ellapsed = difftime(end, start);

	printf("Completed in %.f seconds\n", ellapsed);
}

unsigned char testbuff[1024];

static void test_write_file_async()
{
	uint32_t r;
	size_t sz;
	CALLBACK_CONTEXT context;

	context.completed = 0;
	printf("Writing file asynchronously...");
	time(&context.start);

	/*
	// read a file from the local filesystem and write it to the device
	*/
	context.f = fopen(LOCAL_FILE, "rb");
	fseek(context.f, 0, SEEK_END);
	sz = ftell(context.f);
	rewind(context.f);

	r = sm_file_open(&context.file, "x:\\mrt.exe", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	sm_file_set_buffer(&context.file, testbuff);
	r = sm_file_alloc(&context.file, sz);
	if (r != FAT_SUCCESS)
	{
		printf("prealloc error: 0x%x", r);
	}

	context.bytes_written = 0;		
	context.bytes_read = fread(context.buff, 1, 1024, context.f);
	if (context.bytes_read)
	{
		r = sm_file_write_async(&context.file, context.buff, context.bytes_read, &context.result, &write_file_async_callback, &context);
		while (!context.completed)
			Sleep(1);
	}
	else
	{
		r = sm_file_close(&context.file);
		fclose(context.f);

		time(&context.end);
		context.ellapsed = difftime(context.end, context.start);

		printf("Completed in %.f seconds\n", context.ellapsed);
	}
}

static void write_file_async_callback(CALLBACK_CONTEXT* context, uint16_t* result)
{
	uint16_t r;
	context->bytes_written += context->bytes_read;
	context->bytes_read = fread(context->buff, 1, 1024, context->f);

	if (context->bytes_read)
	{
		r = sm_file_write_async(&context->file, context->buff, context->bytes_read, &context->result, &write_file_async_callback, context);
		return;
	}
	else
	{
		r = sm_file_close(&context->file);
		fclose(context->f);

		time(&context->end);
		context->ellapsed = difftime(context->end, context->start);

		printf("Completed in %.f seconds\n", context->ellapsed);
		context->completed = 1;
	}
}

static void test_read_file_async()
{
	CALLBACK_CONTEXT context;
	uint16_t ret;

	printf("Reading file asynchronously...");
	/*
	// read a file from the device and write it to the local filesystem
	*/
	context.completed = 0;
	context.bytes_read = 0;
	ret = sm_file_open(&context.file, "x:\\mrt.exe", SM_FILE_ACCESS_READ);
	context.f = fopen(LOCAL_FOLDER "mrt_async.exe", "wb");

	sm_file_read_async(&context.file, context.buff, 1024, &context.bytes_read, &context.result, &read_file_async_callback, &context);

	while(!context.completed)
		Sleep(1);

	printf("Completed.\n");

}

static void read_file_async_callback(CALLBACK_CONTEXT* context, uint16_t* result)
{
	uint16_t* r;
	if (context->bytes_read)
	{
		fwrite(context->buff, 1, context->bytes_read, context->f);
		sm_file_read_async(&context->file, context->buff, 1024, &context->bytes_read, &context->result, &read_file_async_callback, context);
	}
	else
	{
		fclose(context->f);
		context->result = sm_file_close(&context->file);
		context->completed = 1;
	}
}



static void test_seek_file()
{
	SM_FILE file;
	uint32_t r;
	unsigned char buff[1024];
	size_t bytes_read;
	size_t sz;
	int i = 0;
	char done = 0;
	FILE* f;
	time_t start;
	time_t end;
	double ellapsed;

	printf("Seeking file...");
	time(&start);

	/*
	// read a file from the local filesystem and write it to the device
	*/
	f = fopen(LOCAL_FILE, "rb");
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);

	r = sm_file_open(&file, "x:\\mrt_seek.exe", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	r = sm_file_alloc(&file, sz);
	if (r != SM_SUCCESS)
	{
		printf("prealloc error: 0x%x", r);
		return;
	}

	do
	{
		bytes_read = fread(buff, 1, 1024, f);

		if (!done && i++ == 1)
		{
			done = 1;
			memset(buff, 0, 1024);
		}

		if (bytes_read)
		{
			r = sm_file_write(&file, buff, bytes_read);
		}
	}
	while (bytes_read);

	r = sm_file_close(&file);
	fclose(f);

	f = fopen(LOCAL_FILE, "rb");
	r = sm_file_open(&file, "x:\\mrt_seek.exe", SM_FILE_ACCESS_WRITE);
	r = sm_file_seek(&file, 1024, SM_SEEK_START);

	bytes_read = fread(buff, 1, 1024, f);
	bytes_read = fread(buff, 1, 1024, f);

	r = sm_file_write(&file, buff, bytes_read);
	r = sm_file_close(&file);
	fclose(f);

	time(&end);
	ellapsed = difftime(end, start);
	printf("Completed in %.f seconds\n", ellapsed);

}

static void test_append_file(char prealloc)
{
	SM_FILE file;
	uint32_t r;
	unsigned char buff[0xFFFF];
	size_t bytes_written = 0;		
	size_t bytes_read;
	size_t sz;
	FILE* f;

	time_t start;
	time_t end;
	double ellapsed;

	printf("Appending file (prealloc:%i)...", prealloc);
	time(&start);

	/*
	// read a file from the local filesystem and write it to the device
	*/
	f = fopen(LOCAL_FILE, "rb");
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);

	r = sm_file_open(&file, "x:\\mrt_apnd.exe", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	/* r = fat_file_alloc(&hFat, sz); */

	bytes_read = fread(buff, 1, 0xFFFe, f);
	if (bytes_read)
	{
		r = sm_file_write(&file, buff, bytes_read);
		bytes_written += bytes_read;
	}
	r = sm_file_close(&file);
	r = sm_file_open(&file, "x:\\mrt_apnd.exe", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_APPEND);
	if (prealloc)
		r = sm_file_alloc(&file, sz);

	do
	{
		bytes_read = fread(buff, 1, prealloc ? 1024 : 0xFFFe, f);
		if (bytes_read)
		{
			r = sm_file_write(&file, buff, bytes_read);
			bytes_written += bytes_read;
		}
	}
	while (bytes_read);
	r = sm_file_close(&file);
	fclose(f);

	time(&end);
	ellapsed = difftime(end, start);

	printf("Completed in %.f seconds\n", ellapsed);
}

static void test_read_file()
{
	SM_FILE file;
	uint32_t r;
	unsigned char buff[102400];
	size_t bytes_read;
	uint32_t total_bytes_read = 0;
	FILE* f;
	SM_DIRECTORY_ENTRY file_entry;

	printf("Reading file...");

	r = sm_get_file_entry("x:\\mrt.exe", &file_entry);

	/*
	// read a file from the device and write it to the local filesystem
	*/
	bytes_read = 0;
	r = sm_file_open(&file, "x:\\mrt.exe", SM_FILE_ACCESS_READ);
	f = fopen(LOCAL_FOLDER "mrt.exe", "wb");
	do
	{
		r = sm_file_read(&file, buff, 1024, &bytes_read);
		fwrite(buff, 1, bytes_read, f);
		total_bytes_read += bytes_read;
	}
	while (bytes_read);
	fclose(f);
	r = sm_file_close(&file);

	if (file_entry.size != total_bytes_read)
	{
		printf("Error: file is reported as %x bytes but we only read %x\n", file_entry.size, total_bytes_read);
	}
	else
	{
		printf("Completed.\n");
	}
}

static void test_read_file_unbuffered()
{
	SM_FILE file;
	uint32_t r;
	unsigned char buff[1024];
	size_t bytes_read;
	FILE* f;
	uint32_t total_bytes_written = 0;

	printf("Reading file (unbuffered)...");
	/*
	// read a file from the device and write it to the local filesystem
	*/
	bytes_read = 0;
	r = sm_file_open(&file, "x:\\mrt.exe", SM_FILE_ACCESS_READ | SM_FILE_FLAG_NO_BUFFERING);
	f = fopen(LOCAL_FOLDER "mrt_unbuffered.exe", "wb");
	do
	{
		r = sm_file_read(&file, buff, 1024, &bytes_read);
		if (r != SM_SUCCESS)
		{
			printf("Read error 0x%x\n", r);
			fclose(f);
			r = sm_file_close(&file);
			return;

		}
		fwrite(buff, 1, bytes_read, f);
		total_bytes_written += bytes_read;
	}
	while (bytes_read);
	fclose(f);
	r = sm_file_close(&file);

	printf("Completed.\n");
}

int file1writes;
unsigned char buff1[4096];

static void test_another_async_write()
{
	int i;
	uint16_t err;
	SM_FILE file;

	//
	// fill buffer with data
	//
	for (i = 0; i < 4096; i++)
	{
		buff1[i] = (i & 1)?'X':'x';
	}
	buff1[0] = 'h';
	buff1[1] = 'e';
	buff1[2] = 'l';
	buff1[3] = 'l';
	buff1[4] = 'o';
	buff1[5] = ' ';
	/*
	// reset the file writes count
	*/
	file1writes = 0;

	printf("Writing another file...");
	err = sm_file_open(&file, "x:\\xxx.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	sm_file_write_async(&file, buff1, 4096, &err, (void*) &test_another_async_write_callback, &file);

	while (file1writes < 20)
		Sleep(1);

	printf("Completed.\n");

}

static void test_another_async_write_callback(SM_FILE* file, uint16_t* result)
{
	file1writes++;
	if (file1writes == 10)
	{
		/*
		// close the file
		*/
		sm_file_close(file);
		file1writes = 20;
	}
	else
	{
		sm_file_write_async(file, buff1, 4096, result, (void*) &test_another_async_write_callback, file);
	}

}

FILE* streamf;
unsigned char streambuff[1024];
char stream_completed;

static void test_write_stream()
{

	int i;
	uint16_t err;
	SM_FILE file;
	size_t sz;

	printf("Writing stream...");
	stream_completed = 0;
	/*
	// open local file
	*/
	streamf = fopen("c:\\stream.old.txt", "rb");
	if (!streamf)
	{
		printf("Could not open local file.\n");
		return;
	}
	fseek(streamf, 0, SEEK_END);
	sz = ftell(streamf);
	rewind(streamf);

	/*
	// open the file
	*/
	err = sm_file_open(&file, "x:\\stream.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE | SM_FILE_FLAG_NO_BUFFERING);
	if (err != SM_SUCCESS)
	{
		printf("Could not open file. Error: 0x%x\n", err);
		return;
	}
	/*
	// allocate disk space
	*/
	err = sm_file_alloc(&file, sz);
	/*
	// read local file
	*/
	err = fread(streambuff, 1, 1024, streamf);
	if (!err)
	{
		printf("Could not read local file.\n");
		return;
	}
	/*
	// initiate stream write
	*/
	err = sm_file_write_stream(&file, streambuff, 1024, &err, (SM_STREAM_CALLBACK) &test_write_stream_callback, &file);
	if (err != FILESYSTEM_OP_IN_PROGRESS)
	{
		printf("Error: 0x%x\n", err);
		return;
	}
	/*
	// wait until it's done
	*/
	while (!stream_completed)
		Sleep(1);
}


static void test_write_stream_callback(SM_FILE* file, uint16_t* result, unsigned char** buffer, uint16_t* action)
{
	if (*result == FILESYSTEM_AWAITING_DATA)
	{
		/*
		// reload the buffer
		*/
		uint16_t bytes;
		bytes = fread(streambuff, 1, 1024, streamf);
		/*
		// if we got data set the response to READY otherwise set it to STOP
		*/
		if (bytes)
		{
			*action = FAT_STREAMING_RESPONSE_READY;
		}
		else
		{
			*action = FAT_STREAMING_RESPONSE_STOP;
		}
	}
	else
	{
		if (*result == SM_SUCCESS)
		{
			printf("Completed.\n");
		}
		else
		{
			printf("Error: 0x%x\n", *result);
		}
		/*
		// close the file
		*/
		sm_file_close(file);
		/*
		// signal that we're done
		*/
		stream_completed = 1;
	}
}