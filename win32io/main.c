/*
 * fat32lib - Portable FAT12/16/32 Filesystem Library
 * Copyright (C) 2013 Fernando Rodriguez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
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
#include "..\fat32lib\fat_utilities.h"
#include "win32io.h"


typedef struct 
{
	char* command;
	int (*command_entry)(char*);
}
COMMAND;

#define MAX_COMMAND_LEN		255

//
// file scope variables
//
static FAT_VOLUME fat_volume;
static STORAGE_DEVICE storage_device;


//
// commands
//
static int cmd_test(char*);
static int cmd_exit(char*);
static int cmd_unknown(char*);
static int cmd_mount(char*);
static int cmd_unmount(char*);
static int parse_args(char* args, char** arguments, int max_args);
//
// tests
//
static void test_directory_struct();
static void test_write_file(char prealloc);
static void test_read_file();
static void test_append_file(char prealloc);
static void test_create_100_files();
static void test_seek_file();
static void test_delete_file();
static void test_rename_file();

static COMMAND commands[] = 
{
	{ "TEST", &cmd_test },
	{ "EXIT", &cmd_exit },
	{ "MOUNT", &cmd_mount },
	{ "UNMOUNT", &cmd_unmount },
	{ "", &cmd_unknown }	// this must always be the last entry
};


static int cmd_test(char* args)
{

	uint16_t r;

	//
	// get the storage device interface
	//
	win32io_get_storage_device(TEXT("\\\\.\\E:"), &storage_device);
	//
	// mount the fat volume
	//
	if ((r = fat_mount_volume(&fat_volume, &storage_device)) == STORAGE_SUCCESS)
	{
		printf("Volume '%s' mounted.\n", fat_volume.label);
	}
	else
	{
		printf("fat32lib error: %x\n", r);
	}

	//test_directory_struct();
	//test_create_100_files();
	//test_read_file();
	//test_write_file(1);
	//test_append_file(1);
	//test_seek_file();
	test_delete_file();
	test_rename_file();

	cmd_unmount("");

	printf("\n");
	
	return 0;
}

static void test_delete_file()
{
	uint16_t ret;

	printf("Deleting file...");

	ret = fat_file_delete(&fat_volume, "\\mrt_seek.exe");
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
	ret = fat_file_rename(&fat_volume, "\\mrt.exe", "\\mrt_renamed.exe");
	if (ret != FAT_SUCCESS)
	{
		printf("Error: %x\n", ret);
		return;
	}
	printf("Done.\n");
}

static void test_directory_struct()
{
	uint16_t r;
	uint16_t i;
	FAT_FILE file;
	char buff[50];
	char hello[] = "Hello World!";
	char goodbye[] = "Goodbye cruel world!";


	r = fat_create_directory(&fat_volume, "\\My New Directory");
	r = fat_create_directory(&fat_volume, "\\My Long Filename Folder");

	r = fat_file_open(&fat_volume, "\\My New Directory\\Hello World.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	r = fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume, "\\My New Directory\\Hello World2.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	r = fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume, "\\My New Directory\\Hello World2.txt", FAT_FILE_ACCESS_APPEND, &file);
	r = fat_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume, "\\Hello World.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	r = fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume, "\\Hello World2.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	r = fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume,
		"\\Hello World2.txt", FAT_FILE_ACCESS_APPEND, &file);
	r = fat_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume, "\\hey.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	r = fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume, "\\bye.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	r = fat_file_write(&file, (unsigned char*) goodbye, strlen(goodbye));
	r = fat_file_close(&file);

	r = fat_file_open(&fat_volume,
		"\\My Long Filename Folder\\hello.txt", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);
	r = fat_file_write(&file, (unsigned char*) hello, strlen(hello));
	r = fat_file_close(&file);

}

static void test_create_100_files()
{
	uint16_t r;
	uint16_t i;
	FAT_FILE file;
	char buff1[50];
	char buff2[50];
	time_t start;
	time_t end;
	double ellapsed;

	printf("Creating 100 files...");
	time(&start);

	//
	// create a directory
	//
	r = fat_create_directory(&fat_volume, "\\Lots of Files");
	if (r != FAT_SUCCESS && r != FAT_FILENAME_ALREADY_EXISTS)
	{
		printf("Could not create folder 'Lots Of Files'. Error: %x\n", r);
	}

	//
	// create 100 files
	//
	for (i = 1; i < 101; i++)
	{
		sprintf(buff1, "\\Lots of Files\\HeLLooooooo%i.txt", i);
		sprintf(buff2, "Hello, my name is HeLLo%i.txt", i);

		r = fat_file_open(&fat_volume, buff1, FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &file);

		if (r == FAT_SUCCESS)
		{
			r = fat_file_write(&file, (unsigned char*) buff2, strlen(buff2));

			if (r == FAT_SUCCESS)
			{
				r = fat_file_close(&file);

				if (r != FAT_SUCCESS)
				{
					printf("Could not close file '%s'. Error: %i\n", buff1, r);
				}
			}
			else
			{
				printf("Could write file '%s'. Error: %i\n", buff1, r);
			}
		}
		else
		{
			printf("Could not open file '%s'. Error: %i\n", buff1, r);
		}
	}
	time(&end);
	ellapsed = difftime(end, start);
	printf("Completed in %.f seconds\n", ellapsed);
}

static void test_write_file(char prealloc)
{
	FAT_FILE hFat;
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

	//
	// read a file from the local filesystem and write it to the device
	//C:\\Users\\Fernan\\Pictures\\gabi.jpg
	//
	f = fopen("C:\\20861e6dae78f969b1\\mrt.exe", "rb");
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);

	r = fat_file_open(&fat_volume, "\\mrt.exe", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &hFat);

	if (prealloc)
		r = fat_file_alloc(&hFat, sz);

	do
	{
		bytes_read = fread(buff, 1, prealloc ? 1024 : 0xFFFe, f);
		if (bytes_read)
		{
			r = fat_file_write(&hFat, buff, bytes_read);
			bytes_written += bytes_read;
		}
	}
	while (bytes_read);
	r = fat_file_close(&hFat);
	fclose(f);

	time(&end);
	ellapsed = difftime(end, start);

	printf("Completed in %.f seconds\n", ellapsed);

}

static void test_seek_file()
{
	FAT_FILE hFat;
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

	printf("Seek file file test...");
	time(&start);

	//
	// read a file from the local filesystem and write it to the device
	//C:\\Users\\Fernan\\Pictures\\gabi.jpg
	//
	f = fopen("C:\\20861e6dae78f969b1\\mrt.exe", "rb");
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);

	r = fat_file_open(&fat_volume, "\\mrt_seek.exe", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &hFat);
	r = fat_file_alloc(&hFat, sz);

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
			r = fat_file_write(&hFat, buff, bytes_read);
		}
	}
	while (bytes_read);

	r = fat_file_close(&hFat);
	fclose(f);

	f = fopen("C:\\20861e6dae78f969b1\\mrt.exe", "rb");
	r = fat_file_open(&fat_volume, "\\mrt_seek.exe", FAT_FILE_ACCESS_WRITE, &hFat);
	r = fat_file_seek(&hFat, 1024, FAT_SEEK_START);

	bytes_read = fread(buff, 1, 1024, f);
	bytes_read = fread(buff, 1, 1024, f);

	r = fat_file_write(&hFat, buff, bytes_read);
	r = fat_file_close(&hFat);
	fclose(f);

	time(&end);
	ellapsed = difftime(end, start);
	printf("Completed in %.f seconds\n", ellapsed);

}

static void test_append_file(char prealloc)
{
	FAT_FILE hFat;
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

	//
	// read a file from the local filesystem and write it to the device
	//C:\\Users\\Fernan\\Pictures\\gabi.jpg
	//
	f = fopen("C:\\20861e6dae78f969b1\\mrt.exe", "rb");
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	rewind(f);

	r = fat_file_open(&fat_volume, "\\mrt_append.exe", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE, &hFat);
	//r = fat_file_alloc(&hFat, sz);

	bytes_read = fread(buff, 1, 0xFFFe, f);
	if (bytes_read)
	{
		r = fat_file_write(&hFat, buff, bytes_read);
		bytes_written += bytes_read;
	}
	r = fat_file_close(&hFat);
	r = fat_file_open(&fat_volume, "\\mrt_append.exe", FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_APPEND, &hFat);
	if (prealloc)
		r = fat_file_alloc(&hFat, sz);

	do
	{
		bytes_read = fread(buff, 1, prealloc ? 1024 : 0xFFFe, f);
		if (bytes_read)
		{
			r = fat_file_write(&hFat, buff, bytes_read);
			bytes_written += bytes_read;
		}
	}
	while (bytes_read);
	r = fat_file_close(&hFat);
	fclose(f);

	time(&end);
	ellapsed = difftime(end, start);

	printf("Completed in %.f seconds\n", ellapsed);
}

static void test_read_file()
{
	FAT_FILE hFat;
	uint32_t r;
	unsigned char buff[102400];
	size_t bytes_read;
	FILE* f;
	//
	// read a file from the device and write it to the local filesystem
	//
	bytes_read = 0;
	r = fat_file_open(&fat_volume, "\\mrt.exe", FAT_FILE_ACCESS_READ, &hFat);
	f = fopen("c:\\mrt.exe", "wb");
	do
	{
		r = fat_file_read(&hFat, buff, 1024, &bytes_read);
		fwrite(buff, 1, bytes_read, f);
	}
	while (bytes_read);
	fclose(f);
	r = fat_file_close(&hFat);
}

static int cmd_mount(char* args)
{
	int c;
	char* argument;

	TCHAR argW[MAX_PATH + 1];

	c = parse_args(args, &argument, 1);

	if (c > 1)
	{
		printf("Too many arguments.\n\n");
		return 0;
	}

	MultiByteToWideChar(CP_ACP, (DWORD)0, argument, strlen(argument) + 1, argW, MAX_PATH);
/*
	time_t now;
	struct tm tm;
	tm.tm_hour = 0;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	tm.tm_mon = 0;
	tm.tm_mday = 0;
	tm.tm_year = 0;
	now = mktime(tm);
*/
	//
	// get the storage device interface
	//
	win32io_get_storage_device(argW, &storage_device);

	if ((c = fat_mount_volume(&fat_volume, &storage_device)) == STORAGE_SUCCESS)
	{
		printf("Volume '%s' mounted.\n\n", fat_volume.label);
	}
	else
	{
		printf("Could not mount volume. Error: %x\n\n", c);
	}

	return 0;
}

static int cmd_unmount(char* args)
{
	memset(&fat_volume, 0, sizeof(fat_volume));
	win32io_release_storage_device();
	printf("\n");
	return 0;
}

static int cmd_unknown(char* args)
{
	printf("Command not recognized.\n\n");
	return 0;
}

static int cmd_exit(char* args)
{
	printf("\n");
	#if !defined(NDEBUG)
	printf("Press ENTER to exit...");
	getch();
	#endif
	return -1;
}

int main() 
{
	int i;
	int len;
	char* args = 0;
	char command[MAX_COMMAND_LEN + 2];

	printf("fat32lib Test Utility Version 1.0\n");
	printf("Copyright (c) 2013 - Fernando Rodriguez\n\n");
	printf("Type 'help' for a list of available commands.\n\n");

	do
	{
		do
		{
			args = 0;
			printf("%s>", fat_volume.label);
			fgets(command, MAX_COMMAND_LEN, stdin);
			len = strlen(command) - 1;	// get length of command minus newline
		}
		while (!len);

		command[len] = 0;			// replace new line with null terminator
		for (i = 0; i < len; i++)
		{
			if (command[i] == 0x20)
			{
				if (len > i) 
				{
					args = &command[i + 1];
					while (*args == 0x20) args++;
				}
				len = i;
				break;
			}
			command[i] = (char) toupper((int) command[i]);
		}

		i = 0;

		do
		{
			if (!strlen(commands[i].command) || !memcmp(command, commands[i].command, len))
			{
				i = commands[i].command_entry(args);
				break;
			}
		}
		while(++i);

	}
	while (i >= 0);

	return i;
}

static int parse_args(char* args, char** arguments, int max_args)
{

	int i;
	char* args_index;
	char* end_of_args;

	if (!args)
		return 0;

	i = 0;
	args_index = args;
	end_of_args = args + strlen(args);
	do
	{
		if (*args_index != 0x20)
		{
			if (i < max_args)
			{
				arguments[i++] = args_index++;
			}
			else 
			{
				i++;
			}
			while (end_of_args > args_index)
			{
				if (*args_index == 0x20)
				{
					*args_index++ = 0x0;
					break;
				}
				args_index++;
			}
		}
		else
		{
			args_index++;
		}
	}
	while (end_of_args > args_index);

	return i;
}
