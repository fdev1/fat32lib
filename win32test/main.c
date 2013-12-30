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
#include "..\win32io\win32io.h"


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
int cmd_test(char*);
static int cmd_exit(char*);
static int cmd_unknown(char*);
static int cmd_mount(char*);
static int cmd_unmount(char*);
static int parse_args(char* args, char** arguments, int max_args);

static COMMAND commands[] = 
{
	{ "TEST", &cmd_test },
	{ "EXIT", &cmd_exit },
	/* { "MOUNT", &cmd_mount }, */
	/* { "UNMOUNT", &cmd_unmount }, */
	{ "", &cmd_unknown }	/* this must always be the last entry */
};


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
	//printf("Type 'help' for a list of available commands.\n\n");
	printf("The only command available at this time is 'test'.\n\n");
	
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
