/*
 * fat32lib - Portable FAT12/16/32 Filesystem Library
 * Copyright (C) 2013 Fernando Rodriguez (frodriguez.developer@outlook.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 3 as 
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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fat.h"
#include "fat_internals.h"

/*
// macros for setting the cached sector
*/
#if defined(FAT_ALLOCATE_SHARED_BUFFER)
#define FAT_SET_LOADED_SECTOR(volume, sector)		fat_shared_buffer_sector = (sector)
#elif
#define FAT_SET_LOADED_SECTOR(volume, sector)		volume->sector_buffer_sector = (sector)
#else
#define FAT_SET_LOADED_SECTOR(volume, sector)	
#endif

/*
// function pointer for rtc access routing
*/
#if !defined(FAT_USE_SYSTEM_TIME)
typedef struct TIMEKEEPER
{
	FAT_GET_SYSTEM_TIME fat_get_system_time;
} 
TIMEKEEPER;
static TIMEKEEPER timekeeper;
#endif

/*
// TODO:
// 1. Optimize fat_file_seek
//
// 2. Update fat_file_alloc to allocated clusters based on the # of byytes
//		needed and not the clusters needed to allocate those bytes. Right now
//		it's overallocating under some circumstances. The extra clusters get
//		freed when the file is closed anyways but it'll still be more efficient
//		that way.
*/

/*
// shared buffer definition
*/
#if defined(FAT_ALLOCATE_SHARED_BUFFER)
#if defined(FAT_MULTI_THREADED)
DEFINE_CRITICAL_SECTION(fat_shared_buffer_lock);
#endif
unsigned char fat_shared_buffer[MAX_SECTOR_LENGTH];
uint32_t fat_shared_buffer_sector;
#endif

/*
// initialize fat driver
*/
void fat_init()
{
	#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
	INITIALIZE_CRITICAL_SECTION(fat_shared_buffer_lock);
	#endif
	#if defined(FAT_ALLOCATE_SHARED_BUFFER)
	fat_shared_buffer_sector = FAT_UNKNOWN_SECTOR;
	#endif
}

/*
// mounts a FAT volume
*/
uint16_t fat_mount_volume(FAT_VOLUME* volume, STORAGE_DEVICE* device) 
{
	uint16_t ret;
	FAT_BPB* bpb;
	uint32_t hidden_sectors = 0;
	FAT_PARTITION_ENTRY* partition_entry;
	char partitions_tried = 0;
	char label[12];
	uint32_t fsinfo_sector;
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	FAT_BPB bpb_storage;
	FAT_PARTITION_ENTRY partition_entry_mem;
	#endif
	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	unsigned char* buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	unsigned char* buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	#endif
	/*
	// set the null terminator.
	*/
	label[11] = 0;
	/*
	// save the storage device handle
	*/
	volume->device = device;
	/*
	// acquire a lock on the buffer
	*/
	#if defined(FAT_MULTI_THREADED)
	INITIALIZE_CRITICAL_SECTION(volume->write_lock);
	volume->readers_count = 0;
	#endif
	#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
	INITIALIZE_CRITICAL_SECTION(volume->sector_buffer_lock);
	ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
	#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
	ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
	#endif
	/*
	// mark the loaded sector as unknown
	*/
	FAT_SET_LOADED_SECTOR(volume, FAT_UNKNOWN_SECTOR);
	/*
	// retrieve the boot sector (sector 0) from the storage device
	*/
	ret = volume->device->read_sector(volume->device->driver, 0x0, buffer);
	if (ret != STORAGE_SUCCESS)
	{
		#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
		LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
		#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
		LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
		#endif
		return FAT_CANNOT_READ_MEDIA;
	}
	/*
	// set the partition entry pointer
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	partition_entry = &partition_entry_mem;
	#else
	partition_entry = (FAT_PARTITION_ENTRY*) (buffer + 0x1BE);
	#endif
retry:
	/*
	// if we've already tried to mount the volume as partitionless
	// try to mount the next partition
	*/
	if (partitions_tried)
	{
		/*
		// if we've already tried all 4 partitions then we're
		// out of luck
		*/
		if (partitions_tried > 4)
		{
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			return FAT_INVALID_FAT_VOLUME;
		}
		/*
		// if we've tried to mount a partition volume (not the unpartioned one)
		// then we must reload sector 0 (MBR)
		*/
		if (partitions_tried > 1)
		{
			/*
			// retrieve the boot sector (sector 0) from the storage device
			*/
			ret = volume->device->read_sector(volume->device->driver, 0x0, buffer);
			if (ret != STORAGE_SUCCESS)
			{
				#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
				LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
				#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
				LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
				#endif
				return FAT_CANNOT_READ_MEDIA;
			}
			/*
			// move to the next partition entry
			*/
			#if !(defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN))
			partition_entry++;
			#endif
		}
		/*
		// load the partition entry
		*/
		#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
		fat_read_partition_entry(partition_entry, buffer + 0x1BE + ((partitions_tried - 1) * 16));
		#endif
		/*
		// remember how many sectors before this partition
		*/
		hidden_sectors = partition_entry->lba_first_sector;
		/*
		// if the partition is marked as inactive move on
		*/
		/*
		if (!partition_entry->status)
		{
			partitions_tried++;
			goto retry;
		}
		*/
		/*
		// make sure the partition doesn't exceeds the physical boundries
		// of the device
		*/
		if (partition_entry->lba_first_sector + 
			partition_entry->total_sectors > volume->device->get_total_sectors(volume->device->driver))
		{
			partitions_tried++;
			goto retry;
		}
		/*
		// retrieve the 1st sector of partition
		*/
		ret = volume->device->read_sector(volume->device->driver, partition_entry->lba_first_sector, buffer);
		if (ret != STORAGE_SUCCESS)
		{
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			return FAT_CANNOT_READ_MEDIA;
		}
	}
	/*
	// set our pointer to the BPB
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	bpb = &bpb_storage;
	fat_read_bpb(bpb, buffer);
	#else
	bpb = (FAT_BPB*) buffer;
	#endif
	/*
	// if the sector size is larger than what this build
	// allows do not mount the volume
	*/
 	if (bpb->BPB_BytsPerSec > MAX_SECTOR_LENGTH)
	{
		#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
		LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
		#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
		LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
		#endif
		return FAT_SECTOR_SIZE_NOT_SUPPORTED;
	}
	/*
	// make sure BPB_BytsPerSec and BPB_SecPerClus are 
	// valid before we divide by them
	*/
	if (!bpb->BPB_BytsPerSec || !bpb->BPB_SecPerClus)
	{
		partitions_tried++;
		goto retry;
	}
	/*
	// make sure that SecPerClus is a power of two
	*/
	ret = bpb->BPB_SecPerClus;
	while (ret != 0x1)
	{
		if (ret & 0x1)
		{
			partitions_tried++;
			goto retry;
		}
		ret >>= 1;
	}
	/*
	// get all the info we need from BPB
	*/
	volume->root_directory_sectors = ((bpb->BPB_RootEntCnt * 32) + (bpb->BPB_BytsPerSec - 1)) / bpb->BPB_BytsPerSec;
	volume->fat_size = (bpb->BPB_FATSz16) ? bpb->BPB_FATSz16 : bpb->BPB_EX.FAT32.BPB_FATSz32;
	volume->no_of_sectors = (bpb->BPB_TotSec16) ? bpb->BPB_TotSec16 : bpb->BPB_TotSec32;	
	volume->no_of_data_sectors = volume->no_of_sectors - (bpb->BPB_RsvdSecCnt + (bpb->BPB_NumFATs * volume->fat_size) + volume->root_directory_sectors);
	volume->no_of_clusters = volume->no_of_data_sectors / bpb->BPB_SecPerClus;
	volume->first_data_sector = bpb->BPB_RsvdSecCnt + hidden_sectors + (bpb->BPB_NumFATs * volume->fat_size) + volume->root_directory_sectors;
	volume->no_of_reserved_sectors = bpb->BPB_RsvdSecCnt + hidden_sectors;
	volume->no_of_bytes_per_serctor = bpb->BPB_BytsPerSec;
	volume->no_of_sectors_per_cluster = bpb->BPB_SecPerClus;
	volume->no_of_fat_tables = bpb->BPB_NumFATs;
	fsinfo_sector = bpb->BPB_EX.FAT32.BPB_FSInfo;
	/*
	// determine the FAT file system type
	*/
	volume->fs_type = (volume->no_of_clusters < 4085) ? FAT_FS_TYPE_FAT12 :
		(volume->no_of_clusters < 65525) ? FAT_FS_TYPE_FAT16 : FAT_FS_TYPE_FAT32;	
	/*
	// sanity check that the FAT table is big enough
	*/
	switch (volume->fs_type)
	{
		case FAT_FS_TYPE_FAT12:
			if (volume->fat_size < 
				(((volume->no_of_clusters + (volume->no_of_clusters >> 1)) + bpb->BPB_BytsPerSec - 1) / bpb->BPB_BytsPerSec))
			{
				partitions_tried++;
				goto retry;
			}
			break;
		case FAT_FS_TYPE_FAT16:
			if (volume->fat_size < 
				(((volume->no_of_clusters * 2) + bpb->BPB_BytsPerSec - 1) / bpb->BPB_BytsPerSec))
			{
				partitions_tried++;
				goto retry;
			}
			break;
		case FAT_FS_TYPE_FAT32:
			if (volume->fat_size < 
				(((volume->no_of_clusters * 4) + bpb->BPB_BytsPerSec - 1) / bpb->BPB_BytsPerSec))
			{
				partitions_tried++;
				goto retry;
			}
			break;
	}
	/*
	// read the volume label from the boot sector
	*/
	if (volume->fs_type == FAT_FS_TYPE_FAT16)
	{
		volume->id = bpb->BPB_EX.FAT16.BS_VolID;
		memcpy(label, bpb->BPB_EX.FAT16.BS_VolLab, 11);
		strtrim(volume->label, label, 11);
	}
	else 
	{
		volume->id = bpb->BPB_EX.FAT32.BS_VolID;
		memcpy(label, bpb->BPB_EX.FAT32.BS_VolLab, 11);
		strtrim(volume->label, label, 11);
	}
	/*
	// if the volume is FAT32 then copy the root
	// entry's cluster from the BPB_RootClus field
	// on the BPB
	*/
	if (volume->fs_type == FAT_FS_TYPE_FAT32)
	{
		volume->root_cluster = bpb->BPB_EX.FAT32.BPB_RootClus;
	}
	else
	{
		volume->root_cluster = 0x0;
	}

	/* 
	// ###############################################
	// NOTE!!!: bpb is no good from this point on!!!!
	// ###############################################
	*/

	/*
	// check that this is a valid FAT partition by comparing the media
	// byte in the BPB with the 1st byte of the fat table
	*/
	{
		unsigned char media = bpb->BPB_Media;
		/*
		// read the 1st sector of the FAT table
		*/
		ret = volume->device->read_sector(volume->device->driver, volume->no_of_reserved_sectors, buffer);
		if (ret != STORAGE_SUCCESS)
		{
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			return FAT_CANNOT_READ_MEDIA;
		}
		/*
		// if the lower byte of the 1st FAT entry is not the same as
		// BPB_Media then this is not a valid volume
		*/
		if (buffer[0] != media)
		{
			partitions_tried++;
			goto retry;
		}
	}
	/*
	// read volume label entry from the root directory (if any)
	*/
	{
		/*
		FAT_FILESYSTEM_QUERY query;
		memset(&query, 0, sizeof(FAT_FILESYSTEM_QUERY));
		if (fat_find_first_entry(volume, 0, FAT_ATTR_VOLUME_ID, 0, &query) == FAT_SUCCESS)
		{
			if (*query.current_entry.name != 0)
			{
				strtrim(volume->label, (char*) query.current_entry.name , 11);
			}
		}
		*/

		FAT_QUERY_STATE_INTERNAL query;
		query.buffer = buffer;
		if (fat_query_first_entry(volume, 0, FAT_ATTR_VOLUME_ID, (FAT_QUERY_STATE*) &query, 1) == FAT_SUCCESS)
		{
			if (*query.current_entry_raw->ENTRY.STD.name != 0)
			{
				strtrim(volume->label, (char*) query.current_entry_raw->ENTRY.STD.name, 11);
			}
		}
	}
	volume->fsinfo_sector = 0xFFFFFFFF;
	/*
	// if we find a valid fsinfo structure we'll use it
	*/
	if (volume->fs_type == FAT_FS_TYPE_FAT32)
	{
		FAT_FSINFO* fsinfo;
		#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
		FAT_FSINFO fsinfo_mem;
		#endif
		/*
		// read the sector containing the FSInfo structure
		*/
		ret = volume->device->read_sector(volume->device->driver, hidden_sectors + fsinfo_sector, buffer);
		if (ret != STORAGE_SUCCESS)
		{
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			return FAT_CANNOT_READ_MEDIA;
		}
		/*
		// set fsinfo pointer
		*/
		#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
		fsinfo = &fsinfo_mem;
		fat_read_fsinfo(fsinfo, buffer);
		#else
		fsinfo = (FAT_FSINFO*) buffer;
		#endif
		/*
		// check signatures before using
		*/
		if (fsinfo->LeadSig == 0x41615252 && fsinfo->StructSig == 0x61417272 && fsinfo->TrailSig == 0xAA550000)
		{
			volume->next_free_cluster = fsinfo->Nxt_Free;
			/*
			// if this value is greater than or equal to the # of
			// clusters in the volume it cannot possible be valid
			*/
			if (fsinfo->Free_Count < volume->no_of_clusters)
			{
				volume->total_free_clusters = fsinfo->Free_Count;
			}
			else
			{
				volume->total_free_clusters = volume->no_of_clusters - 1;
			}
		}
		else
		{
			volume->next_free_cluster = 0xFFFFFFFF;
			volume->total_free_clusters = volume->no_of_clusters - 1;
		}
		/*
		// remember fsinfo sector
		*/
		volume->fsinfo_sector = hidden_sectors + fsinfo_sector;
	}
	/*
	// release the buffer lock
	*/
	#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
	LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
	#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
	LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
	#endif
	/*
	// return success
	*/
	return FAT_SUCCESS;
}

/*
// dismounts a FAT volume
*/
uint16_t fat_dismount_volume(FAT_VOLUME* volume)
{
	/*
	// if this is a FAT32 volume we'll update the fsinfo structure
	*/
	#if !defined(FAT_READ_ONLY)
	if (volume->fs_type == FAT_FS_TYPE_FAT32 && volume->fsinfo_sector != 0xFFFFFFFF)
	{
		uint16_t ret;
		FAT_FSINFO* fsinfo;
		#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
		FAT_FSINFO fsinfo_mem;
		#endif

		#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
		unsigned char* buffer = volume->sector_buffer;
		#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
		unsigned char* buffer = fat_shared_buffer;
		#else
		ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
		#endif
		/*
		// lock the buffer
		*/
		#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
		INITIALIZE_CRITICAL_SECTION(volume->sector_buffer_lock);
		ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
		#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
		ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
		#endif
		/*
		// mark the loaded sector as unknown
		*/
		FAT_SET_LOADED_SECTOR(volume, FAT_UNKNOWN_SECTOR);
		/*
		// read the sector containing the FSInfo structure
		*/
		ret = volume->device->read_sector(volume->device->driver, volume->fsinfo_sector, buffer);
		if (ret != STORAGE_SUCCESS)
		{
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			return FAT_CANNOT_READ_MEDIA;
		}
		/*
		// set the pointer to the fsinfo structure
		*/
		#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
		fsinfo = &fsinfo_mem;
		fat_read_fsinfo(fsinfo, buffer);
		#else
		fsinfo = (FAT_FSINFO*) buffer;
		#endif
		/*
		// check the signatures before writting
		// note: when you mount a removable device in windows it will channge
		// these signatures, i guess it feels it cannot be trusted. So we're going
		// to rebuild them no matter what as they significantly speed up this
		// implementation. After the volume has been mounted elsewhere Free_Count cannot
		// be trusted. This implementation doesn't actually use it but if you only
		// mount the volume with us it will keep it up to date.
		*/
		/*if (fsinfo->LeadSig == 0x41615252 && fsinfo->StructSig == 0x61417272 && fsinfo->TrailSig == 0xAA550000)*/
		{
			/*
			// mark all values as unknown
			*/
			fsinfo->Nxt_Free = volume->next_free_cluster;
			fsinfo->Free_Count = volume->total_free_clusters;
			fsinfo->LeadSig = 0x41615252;
			fsinfo->StructSig = 0x61417272;
			fsinfo->TrailSig = 0xAA550000;
			/*
			// copy fsinfo struct to buffer
			*/
			#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
			fat_write_fsinfo(fsinfo, buffer);
			#endif
			/*
			// write the fsinfo sector
			*/
			ret = volume->device->write_sector(volume->device->driver, volume->fsinfo_sector, buffer);
			if (ret != STORAGE_SUCCESS)
			{
				#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
				LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
				#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
				LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
				#endif
				return FAT_CANNOT_READ_MEDIA;
			}
		}
	}
	#endif
	/*
	// leave critical section and
	// delete the critical section for volume buffer
	*/
	#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
	LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
	DELETE_CRITICAL_SECTION(volume->sector_buffer_lock);
	#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
	LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
	#endif
	/*
	// return success code
	*/
	return FAT_SUCCESS;
}

/*
// gets the sector size of the volume.
*/
uint16_t fat_get_sector_size(FAT_VOLUME* volume)
{
	return volume->no_of_bytes_per_serctor;
}

/*
// registers the function that gets the system time
*/
#if !defined(FAT_USE_SYSTEM_TIME)
void fat_register_system_time_function(FAT_GET_SYSTEM_TIME system_time)
{
	timekeeper.fat_get_system_time = system_time;
}
#endif

/*
// finds the first file in a directory
*/
uint16_t fat_find_first_entry( 
	FAT_VOLUME* volume, 
	char* parent_path, unsigned char attributes, FAT_DIRECTORY_ENTRY** dir_entry, FAT_FILESYSTEM_QUERY* q) {
	
	uint16_t ret;
	FAT_DIRECTORY_ENTRY parent_entry;
	FAT_FILESYSTEM_QUERY_INNER* query = (FAT_FILESYSTEM_QUERY_INNER*) q;
	/*
	// make sure the query has a buffer
	*/
	if (!q->state.buffer)
		q->state.buffer = q->state.buff;
	/*
	// if the path starts with a backlash then advance to
	// the next character
	*/
	if (parent_path)
		if (*parent_path == '\\') *parent_path++;
	/*
	// if a parent was specified...
	*/
	if (parent_path != 0) 
	{
		/*
		// try to get the entry for the parent
		*/
		ret = fat_get_file_entry(volume, parent_path, &parent_entry);
		/*
		// if we were unable to get the parent entry
		// then return the error that we received from
		// fat_get_file_entry
		*/
		if (ret != FAT_SUCCESS)
			return ret;
		/*
		// try to get the 1st entry of the
		// query results
		*/
		ret = fat_query_first_entry( 
			volume, &parent_entry.raw, attributes, &query->state, 0);
	}
	/*
	// if the parent was not supplied then we
	// submit the query without it
	*/
	else
	{
		ret = fat_query_first_entry(volume, 0, attributes, &query->state, 0);
	}
	/*
	// if we cant get the 1st entry then return the
	// error that we received from fat_Query_First_entry
	*/
	if ( ret != FAT_SUCCESS )
		return ret;
	/*
	// if there are no more entries
	*/
	if ( query->state.current_entry_raw == 0 ) {
		/*
		// set the filename of the current entry
		// to 0
		*/
		*query->current_entry.name =	0;
		/*
		// return success
		*/
		return FAT_SUCCESS;
	}
	/*
	// fill the current entry structure with data from
	// the current raw entry of the query
	*/
	fat_fill_directory_entry_from_raw( 
		&query->current_entry, query->state.current_entry_raw);
	/*
	// calculate the sector address of the entry - if
	// query->CurrentCluster equals zero then this is the root
	// directory of a FAT12/FAT16 volume and the calculation is
	// different
	*/
	if ( query->state.current_cluster == 0x0 ) {
		query->current_entry.sector_addr =
			volume->no_of_reserved_sectors + (volume->no_of_fat_tables * volume->fat_size) +
			query->state.current_sector;	
	}
	else 
	{
		query->current_entry.sector_addr = 
			FIRST_SECTOR_OF_CLUSTER( volume, query->state.current_cluster) +
			query->state.current_sector; /* + volume->NoOfSectorsPerCluster; */
	}
	/*
	// calculate the offset of the entry within it's sector
	*/
	#if defined (NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	query->current_entry.sector_offset = query->state.current_entry_raw_offset;
	#else
	query->current_entry.sector_offset = (uint16_t) 
		((uintptr_t) query->state.current_entry_raw) - ((uintptr_t) query->state.buffer);
	#endif
	/*
	// store a copy of the original FAT directory entry
	// within the FAT_DIRECTORY_ENTRY structure that is returned
	// to users
	*/
	query->current_entry.raw = *query->state.current_entry_raw;
	/*
	// if long filenames are enabled copy the filename to the entry
	*/
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	if (*query->current_entry.name != 0)
	{
		if (*query->state.long_filename != 0)
		{
			for (ret = 0; ret < 256; ret++)
			{
				query->current_entry.name[ret] = query->state.long_filename[ret];
				if (query->state.long_filename[ret] == 0)
					break;
			}
		}
	}
	#endif
		

	/*
	// if the user provided a pointer-to-pointer to the
	// result set it to the current entry.
	*/
	if (dir_entry)
		*dir_entry = &query->current_entry;
	/*
	// return success code
	*/		
	return FAT_SUCCESS;
}

/*
// finds the next file in the directory
*/
uint16_t fat_find_next_entry( 
	FAT_VOLUME* volume, FAT_DIRECTORY_ENTRY** dir_entry, FAT_FILESYSTEM_QUERY* q) {
		
	uint16_t ret;
	FAT_FILESYSTEM_QUERY_INNER* query = (FAT_FILESYSTEM_QUERY_INNER*) q;

	/*
	// try to get the next entry of the query
	*/	
	ret = fat_query_next_entry( volume, &query->state, 0, 0);
	/*
	// if we received an error from fat_query_next_entry
	// then we return the error code to the calling function
	*/
	if ( ret != FAT_SUCCESS )
		return ret;
	/*
	// if there are no more entries
	*/
	if ( query->state.current_entry_raw == 0 ) 
	{
		/*
		// set the filename of the current entry
		// to 0
		*/
		*query->current_entry.name =	0;
		/*
		// return success
		*/
		return FAT_SUCCESS;
	}
	/*
	// fill the current entry structure with data from
	// the current raw entry of the query
	*/
	fat_fill_directory_entry_from_raw( 
		&query->current_entry, query->state.current_entry_raw);
	/*
	// calculate the sector address of the entry - if
	// query->CurrentCluster equals zero then this is the root
	// directory of a FAT12/FAT16 volume and the calculation is
	// different
	*/
	if (query->state.current_cluster == 0x0) 
	{
		query->current_entry.sector_addr =
			volume->no_of_reserved_sectors + ( volume->no_of_fat_tables * volume->fat_size ) +
			query->state.current_sector;	
	}
	else 
	{
		query->current_entry.sector_addr = 
			FIRST_SECTOR_OF_CLUSTER( volume, query->state.current_cluster) +
			query->state.current_sector; /* + volume->NoOfSectorsPerCluster; */
	}
	/*
	// calculate the offset of the entry within it's sector
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	query->current_entry.sector_offset = query->state.current_entry_raw_offset;
	#else
	query->current_entry.sector_offset = (uint16_t) 
		((uintptr_t) query->state.current_entry_raw) - ((uintptr_t) query->state.buffer);
	#endif
	/*
	// store a copy of the original FAT directory entry
	// within the FAT_DIRECTORY_ENTRY structure that is returned
	// to users
	*/
	query->current_entry.raw = *query->state.current_entry_raw;

	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	if (*query->current_entry.name != 0)
	{
		if (*query->state.long_filename != 0)
		{
			for (ret = 0; ret < 256; ret++)
			{
				query->current_entry.name[ret] = query->state.long_filename[ret];
				if (query->state.long_filename[ret] == 0)
					break;
			}
		}
	}
	#endif
	if (dir_entry)
		*dir_entry = &query->current_entry;
	/*
	// return success code
	*/		
	return FAT_SUCCESS;
}

/*
// fills a directory entry structure with data taken
// from a raw directory entry
*/
void INLINE fat_fill_directory_entry_from_raw(
	FAT_DIRECTORY_ENTRY* entry, FAT_RAW_DIRECTORY_ENTRY* raw_entry ) {
		
	/*
	// copy the filename and transform the filename
	// from the internal structure to the public one
	*/
	fat_get_short_name_from_entry(entry->name , raw_entry->ENTRY.STD.name);
	/*
	// copy other data from the internal entry structure
	// to the public one
	*/	
	entry->attributes = raw_entry->ENTRY.STD.attributes;
	entry->size = raw_entry->ENTRY.STD.size;
	entry->create_time = fat_decode_date_time(raw_entry->ENTRY.STD.create_date, raw_entry->ENTRY.STD.create_time);
	entry->modify_time = fat_decode_date_time(raw_entry->ENTRY.STD.modify_date, raw_entry->ENTRY.STD.modify_time);
	entry->access_time = fat_decode_date_time(raw_entry->ENTRY.STD.access_date, 0);
	entry->raw = *raw_entry;
		
}	

/*
// creates a directory
*/
uint16_t fat_create_directory(FAT_VOLUME* volume, char* directory)
{
	#if defined(FAT_READ_ONLY)
	return FAT_FEATURE_NOT_SUPPORTED;
	#else
	uint16_t ret;
	FAT_DIRECTORY_ENTRY entry;
	/*
	// check that we got a valid pathname
	*/
	if (!directory || strlen(directory) > FAT_MAX_PATH)
		return FAT_INVALID_FILENAME;
	/*
	// try get the file entry
	*/
	ret = fat_get_file_entry(volume, directory, &entry);
	if ( ret != FAT_SUCCESS )
		return ret;
	/*
	// if we don'tfind a file or directory by that name
	// we can create it, otherwise return file already exists error
	*/
	if (*entry.name == 0)
	{
		/*
		// allocate memory for the file path
		*/
		size_t path_len;
		char* path_scanner;
		char file_path[FAT_MAX_PATH + 1];
		FAT_DIRECTORY_ENTRY parent_entry;			
		/*
		// get the name of the file path including 
		// the filename
		*/
		path_len = strlen(directory);
		/*
		// set the pointer that will be used to scan 
		// filename to the end of the filename
		*/
		path_scanner = directory + ( path_len - 0x1 );
		/*
		// if the filename ends with a backslash then it
		// is an invalid filename ( it's actually a directory
		// path )
		*/
		if (*path_scanner == BACKSLASH)
			return FAT_INVALID_FILENAME;
		/*
		// scan the filename starting at the end until
		// a backslash is found - when we exit this loop
		// path_scanner will point to the last character
		// of the filepath
		*/
		while (*(--path_scanner) != BACKSLASH);	/*scan file backwords until we get to */
		/*
		// calculate the length of the path part of the
		// filename
		*/
		path_len = (size_t) (path_scanner - directory);
		/*
		// copy the path part of the filename to
		// the file_path buffer
		*/
		memcpy(file_path, directory, path_len);
		/*
		// set the null terminator of the file_path buffer
		*/
		file_path[path_len] = 0x0;
		/*
		// increase pointer to the beggining of the filename
		// part of the path
		*/			
		path_scanner++;
		/*
		// try to get the entry for the parent directory
		*/
		ret = fat_get_file_entry(volume, file_path, &parent_entry);
		/*
		// if fat_get_file_entry returned an error
		// then we return the error code to the calling
		// function
		*/
		if (ret != FAT_SUCCESS)
			return ret;
		/*
		// if the parent directory does not exists
		*/
		if (*parent_entry.name == 0)
			return FAT_DIRECTORY_DOES_NOT_EXIST;
		/*
		// try to create the directory entry
		*/
		return fat_create_directory_entry(volume, 
			&parent_entry.raw , path_scanner, FAT_ATTR_DIRECTORY, 0, &entry);
	}
	/*
	// if we get here it means that a file or
	// directory with that name already exists.
	*/
	return FAT_FILENAME_ALREADY_EXISTS;
	#endif
}

/*
// gets a FAT_DIRECTORY_ENTRY by it's full path
*/
uint16_t fat_get_file_entry(FAT_VOLUME* volume, char* path, FAT_DIRECTORY_ENTRY* entry) 
{
	uint16_t ret;
	char match;
	unsigned char target_file[13];
	unsigned char* pLevel;
	FAT_RAW_DIRECTORY_ENTRY* current_entry;
	FAT_QUERY_STATE_INTERNAL query;
	/* FAT_QUERY_STATE query; */

	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	char using_lfn;
	/* char using_lfn_and_short; */
	uint16_t target_file_long[FAT_MAX_PATH + 1];	/* stores the utf16 long filename */
	unsigned char current_level[FAT_MAX_PATH + 1];
	#else
	unsigned char current_level[13];
	/* unsigned char target_file[13]; */
	#endif

	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	query.buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	query.buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	query.buffer = buffer;
	#endif


	/*
	// if the path starts with a backlash then advance to
	// the next character
	*/
	if (*path == '\\') *path++;

	if (*path != 0) 
	{
	
		/*
		// set current_entry to 0, in this state
		// it represents the root directory of the 
		// volume
		*/
		current_entry = 0;
		
	}
	/*
	// if the caller did not supply a path then the
	// request is for the root directory, since there's
	// no physical entry for the root directory we must
	// create a fake one
	*/
	else 
	{
		/*
		// copy the file name to the entry and the raw
		// entry in their respective formats
		*/
		strcpy((char*) entry->name, "ROOT");
		get_short_name_for_entry(entry->raw.ENTRY.STD.name, entry->name, 1);
		/*
		// set the general fields of the entry
		*/
		entry->attributes = entry->raw.ENTRY.STD.attributes = FAT_ATTR_DIRECTORY;
		entry->size = entry->raw.ENTRY.STD.size = 0x0;
		/*
		// since the entry does not physically exist the
		// address fields are set to zero as well
		*/	
		entry->sector_addr = 0x0;
		entry->sector_offset = 0x0;
				
		/*
		// set the location of the root directory
		*/
		if (volume->fs_type == FAT_FS_TYPE_FAT32) 
		{	
			/*
			// if this is a FAT32 volume then the root
			// directory is located on the data section just like
			// any other directory
			*/
			entry->raw.ENTRY.STD.first_cluster_lo = LO16(volume->root_cluster);
			entry->raw.ENTRY.STD.first_cluster_hi = HI16(volume->root_cluster);
		}
		else 
		{
			/*
			// if the volume is FAT12/16 we set the cluster
			// address to zero and when time comes to get the
			// directory we'll calculate the address right after
			// the end of the FATs
			*/
			entry->raw.ENTRY.STD.first_cluster_lo = 0x0;
			entry->raw.ENTRY.STD.first_cluster_hi = 0x0;	
		}
		/*
		// return success code
		*/ 
		return FAT_SUCCESS;
	}	
	/*
	// acquire a lock on the buffer
	*/
	#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
	ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
	#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
	ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
	#endif
	/*
	// mark the cached sector as unknown
	*/
	FAT_SET_LOADED_SECTOR(volume, FAT_UNKNOWN_SECTOR);
	/*
	// for each level on the path....
	*/
	do 
	{
		/*
		// set the pointer current level name buffer
		*/	
		pLevel = current_level;		
		/*
		// if the path starts with a backlash then advance to
		// the next character
		*/
		if (*path == '\\') *path++;
		/*
		// copy the name of the current level entry
		*/
		ret = 0;
		while (*path != 0x0 && *path != '\\') 
		{
			#if !defined(FAT_DISABLE_LONG_FILENAMES)
			if (ret++ > FAT_MAX_PATH)
				#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
				LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
				#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
				LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
				#endif
				return FAT_INVALID_FILENAME;
			#else
			if (++ret > 12)
				return FAT_INVALID_FILENAME;
			#endif
			*pLevel++ = *path++;
		}
		*pLevel = 0x0;
		/*
		// try to find the first entry
		*/
		ret = fat_query_first_entry(volume, current_entry, 0, (FAT_QUERY_STATE*) &query, 1);
		/*
		// if we could not find the entry then
		// return an error code
		*/
		if (ret != FAT_SUCCESS) 
		{
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			return ret;
		}
		/*
		// if the output of fat_query_first_entry indicates that
		// there are no entries available...
		*/
		if (*query.current_entry_raw->ENTRY.STD.name == 0x0) 
		{
			/*
			// set the name of the entry to 0
			*/
			*entry->name = 0;
			/*
			// unlock buffer
			*/
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			/*
			// return success
			*/
			return FAT_SUCCESS;
		}
		/*
		// get an LFN version of the filename
		*/
		#if !defined(FAT_DISABLE_LONG_FILENAMES)
		using_lfn = 0;
		/*/using_lfn_and_short = 0;*/
		/*
		// format the current level filename to the 8.3 format
		// if this is an invalid 8.3 filename try to get the LFN
		// once we get a valid filename (either short or LFN) compare
		// it to the one on the current query entry
		*/
		if (get_short_name_for_entry(target_file, current_level, 1) == FAT_INVALID_FILENAME)
		{	
			if (get_long_name_for_entry(target_file_long, current_level) == FAT_INVALID_FILENAME)
			{
				#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
				LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
				#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
				LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
				#endif
				return FAT_INVALID_FILENAME;
			}
			using_lfn = 1;
			match = fat_compare_long_name(target_file_long, query.long_filename)
				|| fat_compare_short_name(target_file, query.current_entry_raw->ENTRY.STD.name);
		}
		else
		{
			match = fat_compare_short_name(target_file, query.current_entry_raw->ENTRY.STD.name);
		}
		#else
		/*
		// format the current level filename to the 8.3 format
		// if this is an invalid filename return error
		*/
		if (get_short_name_for_entry(target_file, current_level, 0) == FAT_INVALID_FILENAME)
		{
			#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
			LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
			#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
			LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
			#endif
			return FAT_INVALID_FILENAME;
		}
		/*
		// match the filename against the current entry
		*/
		match = fat_compare_short_name(target_file, query.current_entry_raw->ENTRY.STD.name);
		#endif
		/*
		// if the file doesn't match then get the
		// next file
		*/
		while (!match) 
		{
			/*
			//  try to get the next file
			*/
			ret = fat_query_next_entry(volume, (FAT_QUERY_STATE*) &query, 1, 0);
			/*
			// if we received an error message then return
			// it to the calling function
			*/
			if (ret != FAT_SUCCESS)
			{
				#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
				LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
				#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
				LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
				#endif
				return ret;
			}
			/*
			// if the output of fat_query_first_entry indicates that
			// there are no entries available then set the entry name to 0
			// and return success
			*/
			if (IS_LAST_DIRECTORY_ENTRY(query.current_entry_raw))
			{
				*entry->name = 0;
				#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
				LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
				#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
				LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
				#endif
				return FAT_SUCCESS;
			}
			/*
			// match the filename against the next entry
			*/
			#if !defined(FAT_DISABLE_LONG_FILENAMES)
			if (using_lfn)
			{
				match = fat_compare_long_name(target_file_long, query.long_filename)
					|| fat_compare_short_name(target_file, query.current_entry_raw->ENTRY.STD.name);
			}
			else
			{
				match = fat_compare_short_name(target_file, query.current_entry_raw->ENTRY.STD.name);
			}
			#else
			match = fat_compare_short_name(target_file, query.current_entry_raw->ENTRY.STD.name);
			#endif
		}
		/*
		// set the current entry to the entry
		// that we've just found
		*/
		current_entry = query.current_entry_raw;
	}
	while (*path != 0x0);
	/*
	// unlock buffer
	*/
	#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
	LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
	#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
	LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
	#endif
	/*
	// copy the filename and transform the filename
	// from the internal structure to the public one
	*/
	fat_get_short_name_from_entry(entry->name, query.current_entry_raw->ENTRY.STD.name);
	/*
	// copy other data from the internal entry structure
	// to the public one
	*/	
	entry->attributes = query.current_entry_raw->ENTRY.STD.attributes;
	entry->size = query.current_entry_raw->ENTRY.STD.size;
	entry->create_time = fat_decode_date_time(query.current_entry_raw->ENTRY.STD.create_date, query.current_entry_raw->ENTRY.STD.create_time);
	entry->modify_time = fat_decode_date_time(query.current_entry_raw->ENTRY.STD.modify_date, query.current_entry_raw->ENTRY.STD.modify_time);
	entry->access_time = fat_decode_date_time(query.current_entry_raw->ENTRY.STD.access_date, 0);
	/*
	// calculate the sector address of the entry - if
	// query->CurrentCluster equals zero then this is the root
	// directory of a FAT12/FAT16 volume and the calculation is
	// different
	*/
	if ( query.current_cluster == 0x0 ) 
	{
		entry->sector_addr =
			volume->no_of_reserved_sectors + (volume->no_of_fat_tables * volume->fat_size) +
			query.current_sector;	
	}
	else 
	{
		entry->sector_addr = 
			FIRST_SECTOR_OF_CLUSTER( volume, query.current_cluster) +
			query.current_sector; /*  + volume->NoOfSectorsPerCluster; */
	}
	/*
	// calculate the offset of the entry within it's sector
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	entry->sector_offset = query.current_entry_raw_offset;
	#else
	entry->sector_offset = (uint16_t) 
		((uintptr_t) query.current_entry_raw) - ((uintptr_t) query.buffer);
	#endif
	/*
	// store a copy of the original FAT directory entry
	// within the FAT_DIRECTORY_ENTRY structure that is returned
	// to users
	*/
	entry->raw = *query.current_entry_raw;
	/*
	// return success.
	*/
	return FAT_SUCCESS;
}
	

/*
// initializes a query of a set of directory
// entries
*/
uint16_t fat_query_first_entry
(
	FAT_VOLUME* volume,
	FAT_RAW_DIRECTORY_ENTRY* directory, 
	unsigned char attributes, 
	FAT_QUERY_STATE* query,
	char buffer_locked
) 
{
	uint16_t ret;
	uint32_t first_sector;
	/* char pass; */
	/*
	// make sure the long filename is set to an empty string
	*/
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	query->long_filename[0] = 0;
	#endif
	/*
	// if the directory entry has the cluster # set to
	// zero it is the root directory so we need to
	// get the cluster # accordingly
	*/
	if (directory)
	{
		if (directory->ENTRY.STD.first_cluster_hi == 0 && directory->ENTRY.STD.first_cluster_lo == 0)
		{
			directory = 0;
		}
	}
	/*
	// if no directory entry was provided
	// we'll use the root entry of the volume
	*/	
	if (directory == 0) 
	{
		/*
		// calculate the cluster # from the
		*/
		if (volume->fs_type == FAT_FS_TYPE_FAT32) 
		{
			query->current_cluster = volume->root_cluster;
			first_sector = FIRST_SECTOR_OF_CLUSTER(volume, query->current_cluster);
		}
		else  
		{
			query->current_cluster = 0x0;
			first_sector = volume->no_of_reserved_sectors + (volume->no_of_fat_tables * volume->fat_size);
		}
	}
	/*
	// if a directory entry was provided
	*/
	else 
	{
		/*
		// if the entry provided is not a directory
		// entry return an error code
		*/
		if (!(directory->ENTRY.STD.attributes & FAT_ATTR_DIRECTORY))
			return FAT_NOT_A_DIRECTORY;
		/*
		// set the CurrentCluster field of the query
		// state structure to the values found on the
		// directory entry structure
		*/
		((uint16_t*) &query->current_cluster)[INT32_WORD0] = directory->ENTRY.STD.first_cluster_lo;
		/*
		// read the upper word of the cluster address
		// only if this is a FAT32 volume
		*/
		if ( volume->fs_type == FAT_FS_TYPE_FAT32 ) 
		{
			((unsigned char*) &query->current_cluster)[INT32_BYTE2] = LO8(directory->ENTRY.STD.first_cluster_hi);
			((unsigned char*) &query->current_cluster)[INT32_BYTE3] = HI8(directory->ENTRY.STD.first_cluster_hi);
		}
		else
		{
			((uint16_t*) &query->current_cluster)[INT32_WORD1] = 0;
		}
		/*
		// get the 1st sector of the directory entry
		*/
		first_sector = 
			FIRST_SECTOR_OF_CLUSTER(volume, query->current_cluster);
	}
	
	/*
	// read the sector into the query
	// state buffer
	*/
	ret = volume->device->read_sector(volume->device->driver, first_sector, query->buffer);
	if (ret != STORAGE_SUCCESS)
		return FAT_CANNOT_READ_MEDIA;
	/*
	// set the 1st and current entry pointers
	// on the query state to the 1st entry of the
	// directory
	*/
	query->Attributes = attributes;
	query->current_sector = 0;

	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	query->current_entry_raw = &query->current_entry_raw_mem;
	query->current_entry_raw_offset = 0;
	fat_read_raw_directory_entry(query->current_entry_raw, query->buffer);
	#else
	query->first_entry_raw = ( FAT_RAW_DIRECTORY_ENTRY* ) query->buffer;
	query->current_entry_raw = ( FAT_RAW_DIRECTORY_ENTRY* ) query->buffer;
	#endif
	/*
	// find the 1st entry and return it's result code
	*/
	return fat_query_next_entry(volume, query, buffer_locked, 1);

	#if defined(UNNECESSARILY_LARGE_CODE)
	/*
	// if the directory is empty...
	*/
	if (*query->current_entry_raw->ENTRY.STD.name == 0x0)
	{
		/*
		// return success
		*/	
		return FAT_SUCCESS;
	}
	/*
	// if this is a long filename entry...
	*/
	if (query->current_entry_raw->ENTRY.STD.attributes == FAT_ATTR_LONG_NAME)
	{
		#if !defined(FAT_DISABLE_LONG_FILENAMES)
		/*
		// if this enntry is marked as the 1st LFN entry
		*/
		if (query->current_entry_raw->ENTRY.LFN.lfn_sequence & FAT_FIRST_LFN_ENTRY)
		{
			query->lfn_sequence = (query->current_entry_raw->ENTRY.LFN.lfn_sequence ^ FAT_FIRST_LFN_ENTRY) + 1;
			query->lfn_checksum = query->current_entry_raw->ENTRY.LFN.lfn_checksum;
			/*
			// insert null terminator at the end of the long filename
			*/
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 2) * 13) + 0xD])[INT16_BYTE0] = 0;
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 2) * 13) + 0xD])[INT16_BYTE1] = 0;

		}
		/*
		// if this is the LFN that we're expecting then
		// process it, otherwise we'll have to wait for 
		// another 1st LFN entry otherwise read the LFN
		// chrs and save them on the query state struct
		*/
		if (query->lfn_checksum == query->current_entry_raw->ENTRY.LFN.lfn_checksum &&
			(query->lfn_sequence == (query->current_entry_raw->ENTRY.LFN.lfn_sequence & (0xFF ^ FAT_FIRST_LFN_ENTRY)) + 1))
		{
			query->lfn_sequence = query->current_entry_raw->ENTRY.LFN.lfn_sequence & (0xFF ^ FAT_FIRST_LFN_ENTRY );
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x0])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[0];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x0])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[1];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x1])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[2];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x1])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[3];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x2])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[4];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x2])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[5];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x3])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[6];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x3])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[7];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x4])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[8];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x4])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[9];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x5])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[0];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x5])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[1];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x6])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[2];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x6])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[3];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x7])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[4];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x7])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[5];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x8])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[6];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x8])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[7];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x9])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[8];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x9])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[9];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xA])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[10];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xA])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[11];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xB])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[0];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xB])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[1];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xC])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[2];
			((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xC])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[3];
		}
		else
		{
			query->lfn_checksum = 0;
		}
		#endif
		/*
		// make sure we don't return this entry
		*/
		pass = (query->Attributes == FAT_ATTR_LONG_NAME);
	}
	else
	{
		/*
		// check that the current entry passes the query
		// attributes check
		*/
		pass = 
			(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_HIDDEN) || (attributes & FAT_ATTR_HIDDEN)) &&
			(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_SYSTEM) || (attributes & FAT_ATTR_SYSTEM)) &&
			(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_VOLUME_ID) || (attributes & FAT_ATTR_VOLUME_ID)) &&
			(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_LONG_NAME) || (attributes & FAT_ATTR_LONG_NAME));
	}
	/*
	// if the current entry is not in use or the entry's
	// attributes do not exactly match the ones supplied to
	// this query...
	*/
	if ( !pass || *query->current_entry_raw->ENTRY.STD.name == 0xE5 ) 
	{
		/*
		// call fat_query_next_entry to get the 1st
		// entry that meets the criteria
		*/
		ret = fat_query_next_entry(volume, query, buffer_locked);
		/*
		// if we received an error from fat_query_next_entry
		// return the error code to the calling function
		*/
		if ( ret != FAT_SUCCESS )
			return ret;
	}
	/*
	// if this entry doesn't have an LFN entry but its marked as having
	// a lowercase name or extension then fill the long filename with the
	// lowercase version
	*/	
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	if (query->long_filename[0] == 0 && 
		(query->current_entry_raw->ENTRY.STD.reserved & (FAT_LOWERCASE_EXTENSION | FAT_LOWERCASE_BASENAME)))
	{
		int i = 0;
		for (ret = 0; ret < 8; ret++)
		{
			if (query->current_entry_raw->ENTRY.STD.name[ret] != 0x20)
			{
				if (query->current_entry_raw->ENTRY.STD.reserved & FAT_LOWERCASE_BASENAME)
				{
					query->long_filename[i++] = tolower(query->current_entry_raw->ENTRY.STD.name[ret]);
				}
				else
				{
					query->long_filename[i++] = query->current_entry_raw->ENTRY.STD.name[ret];
				}
			}
		}
		if (query->current_entry_raw->ENTRY.STD.name[8] != 0x20)
		{
			query->long_filename[i++] = '.';

			for (ret = 8; ret < 11; ret++)
			{
				if (query->current_entry_raw->ENTRY.STD.name[ret] != 0x20)
				{
					if (query->current_entry_raw->ENTRY.STD.reserved & FAT_LOWERCASE_EXTENSION)
					{
						query->long_filename[i++] = tolower(query->current_entry_raw->ENTRY.STD.name[ret]);
					}
					else
					{
						query->long_filename[i++] = query->current_entry_raw->ENTRY.STD.name[ret];
					}
				}
			}
		}
		query->long_filename[i] = 0x0;
	}
	#endif
	/*
	// return success code
	*/
	return FAT_SUCCESS;
	#endif
}
/*
// moves a query to the next entry
*/
uint16_t fat_query_next_entry(FAT_VOLUME* volume, FAT_QUERY_STATE* query, char buffer_locked, char first_entry) 
{
	char pass;
	uint16_t ret;
	uint32_t sector_address;
		
	do 
	{
		/*
		// if the current entry is the last entry of
		// the sector...
		*/
		if (!first_entry)
		{
			#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
			if (query->current_entry_raw_offset == volume->no_of_bytes_per_serctor - 0x20)
			#else
			if (((uintptr_t) query->current_entry_raw - (uintptr_t) query->first_entry_raw) == volume->no_of_bytes_per_serctor - 0x20) 
			#endif
			{
				/*
				// if the current sector is the last of the current cluster then we must find the next
				// cluster... if CurrentCluster == 0 then this is the root directory of a FAT16/FAT12 volume, that
				// volume has a fixed size in sectors and is not allocated as a cluster chain so we don't do this
				*/
				if (query->current_cluster > 0 &&/*query->current_sector > 0x0 &&*/ query->current_sector == volume->no_of_sectors_per_cluster - 1) 
				{
					FAT_ENTRY fat;
					/*
					// if the buffer was locked by the caller then we need to unlock
					// it before calling fat_get_cluster_entry
					*/
					if (buffer_locked)
					{
						#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
						LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
						#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
						LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
						#endif
					}
					/*
					// get the fat structure for the current cluster 
					// and return UNKNOWN_ERROR if the operation fails
					*/			
					if (fat_get_cluster_entry(volume, query->current_cluster, &fat ) != FAT_SUCCESS)
					{
						/*
						// if the buffer was locked by the caller then we need to re-lock
						// it before leaving
						*/
						if (buffer_locked)
						{
							#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
							ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
							#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
							ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
							#endif
						}
						return FAT_UNKNOWN_ERROR;
					}
					/*
					// if the buffer was locked by the caller then we need to re-lock it
					*/
					if (buffer_locked)
					{
						#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
						ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
						#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
						ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
						#endif
					}
					/*
					// if this is the last cluster of the directory...
					*/
					if (fat_is_eof_entry(volume, fat)) 
					{
						/*
						// set the current entry to 0
						*/
						*query->current_entry_raw->ENTRY.STD.name = 0;	
						/*
						// and return success
						*/
						return FAT_SUCCESS;				
					}
					/*
					// set the current cluster to the next
					// cluster of the directory entry
					*/
					query->current_cluster = fat;
					/*
					// reset the current sector
					*/
					query->current_sector = 0x0;
					/*
					// calculate the address of the next sector
					*/
					sector_address = 
						FIRST_SECTOR_OF_CLUSTER(volume, query->current_cluster) + query->current_sector;
				}
				/*
				// if there are more sectors on the current cluster then
				*/
				else 
				{
					/*
					// increase the current sector #
					*/				
					query->current_sector++;
					/*
					// if this is the root directory of a FAT16/FAT12
					// volume and we have passed it's last sector then
					// there's no more entries...
					*/
					if (query->current_cluster == 0x0) 
					{
						if (query->current_sector == volume->root_directory_sectors)
						{
							*query->current_entry_raw->ENTRY.STD.name = 0;
							return FAT_SUCCESS;						
						}
						sector_address = 
							(volume->no_of_reserved_sectors + (volume->no_of_fat_tables * volume->fat_size)) + query->current_sector;
					}
					else
					{
						/*
						// calculate the address of the next sector
						*/
						sector_address = FIRST_SECTOR_OF_CLUSTER(volume, query->current_cluster) + query->current_sector;
					}
				}
				/*
				// read the next sector into the query buffer
				*/
				ret = volume->device->read_sector(volume->device->driver, sector_address, query->buffer);
				if (ret != STORAGE_SUCCESS)
					return FAT_CANNOT_READ_MEDIA;
				/*
				// set the 1st and current entry pointers
				// on the query state to the 1st entry of the
				// directory
				*/
				#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
				query->current_entry_raw_offset = 0;
				fat_read_raw_directory_entry(query->current_entry_raw, query->buffer);
				#else
				query->first_entry_raw = (FAT_RAW_DIRECTORY_ENTRY*) query->buffer;
				query->current_entry_raw = (FAT_RAW_DIRECTORY_ENTRY*) query->buffer;
				#endif
			}
			/*
			// if there are more entries on the current sector...
			*/
			else 
			{
				#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
				/*
				// read the next directory entry from the buffer
				*/
				query->current_entry_raw_offset += 0x20;
				fat_read_raw_directory_entry(query->current_entry_raw, query->buffer + query->current_entry_raw_offset);
				#else
				/*
				// simply increase the current entry pointer
				*/
				query->current_entry_raw++;
				#endif
			}
		}
		else
		{
			first_entry = 0;
		}
		/*
		// if this is a long filename entry...
		*/
		if (query->current_entry_raw->ENTRY.STD.attributes == FAT_ATTR_LONG_NAME && !IS_FREE_DIRECTORY_ENTRY(query->current_entry_raw))
		{
			#if !defined(FAT_DISABLE_LONG_FILENAMES)
			/*
			// if this enntry is marked as the 1st LFN entry
			*/
			if (query->current_entry_raw->ENTRY.LFN.lfn_sequence & FAT_FIRST_LFN_ENTRY)
			{
				query->lfn_sequence = (query->current_entry_raw->ENTRY.LFN.lfn_sequence ^ FAT_FIRST_LFN_ENTRY) + 1;
				query->lfn_checksum = query->current_entry_raw->ENTRY.LFN.lfn_checksum;
				/*
				// insert null terminator at the end of the long filename
				*/
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 2) * 13) + 0xD])[INT16_BYTE0] = 0;
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 2) * 13) + 0xD])[INT16_BYTE1] = 0;
			}
			/*
			// if this is the LFN that we're expecting then
			// process it, otherwise we'll have to wait for 
			// another 1st LFN entry otherwise read the LFN
			// chrs and save them on the query state struct
			*/
			if (query->lfn_checksum == query->current_entry_raw->ENTRY.LFN.lfn_checksum &&
				(query->lfn_sequence == (query->current_entry_raw->ENTRY.LFN.lfn_sequence & (0xFF ^ FAT_FIRST_LFN_ENTRY)) + 1))
			{
				query->lfn_sequence = query->current_entry_raw->ENTRY.LFN.lfn_sequence & (0xFF ^ FAT_FIRST_LFN_ENTRY );
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x0])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[0];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x0])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[1];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x1])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[2];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x1])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[3];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x2])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[4];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x2])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[5];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x3])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[6];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x3])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[7];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x4])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[8];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x4])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_1[9];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x5])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[0];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x5])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[1];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x6])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[2];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x6])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[3];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x7])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[4];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x7])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[5];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x8])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[6];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x8])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[7];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x9])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[8];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0x9])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[9];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xA])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[10];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xA])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_2[11];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xB])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[0];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xB])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[1];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xC])[INT16_BYTE0] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[2];
				((unsigned char*) &query->long_filename[((query->lfn_sequence - 1) * 13) + 0xC])[INT16_BYTE1] = query->current_entry_raw->ENTRY.LFN.lfn_chars_3[3];
			}
			else
			{
				query->lfn_checksum = 0;
			}
			#endif
			/*
			// make sure we never return this entry
			*/
			pass = (query->Attributes == FAT_ATTR_LONG_NAME);
		}
		else
		{
			/*
			// check that the current entry passes the query
			// attributes check
			*/
			pass = 
				(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_HIDDEN ) || (query->Attributes & FAT_ATTR_HIDDEN)) &&
				(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_SYSTEM ) || (query->Attributes & FAT_ATTR_SYSTEM)) &&
				(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_VOLUME_ID ) || (query->Attributes & FAT_ATTR_VOLUME_ID)) &&
				(!(query->current_entry_raw->ENTRY.STD.attributes & FAT_ATTR_LONG_NAME ) || (query->Attributes & FAT_ATTR_LONG_NAME));
		}
	}
	/*
	// repeat the process until we find a valid entry
	// that matches the attributes given
	*/
	while (!pass || *query->current_entry_raw->ENTRY.STD.name == 0xE5);
	/*
	// if we found an entry we need to check it's LFN checksum
	// to make sure that the long filename that we've associated
	// with it belongs to it. If it doesn't clear it.
	*/
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	if (*query->current_entry_raw->ENTRY.STD.name != 0x0)
	{
		if (query->lfn_checksum != fat_long_entry_checksum((unsigned char*)query->current_entry_raw->ENTRY.STD.name))
		{
			query->long_filename[0] = 0x0;
		}
	}
	/*
	// if this entry doesn't have an LFN entry but its marked as having
	// a lowercase name or extension then fill the long filename with the
	// lowercase version
	*/	
	if (query->long_filename[0] == 0 && 
		(query->current_entry_raw->ENTRY.STD.reserved & (FAT_LOWERCASE_EXTENSION | FAT_LOWERCASE_BASENAME)))
	{
		int i = 0;
		for (ret = 0; ret < 8; ret++)
		{
			if (query->current_entry_raw->ENTRY.STD.name[ret] != 0x20)
			{
				if (query->current_entry_raw->ENTRY.STD.reserved & FAT_LOWERCASE_BASENAME)
				{
					query->long_filename[i++] = tolower(query->current_entry_raw->ENTRY.STD.name[ret]);
				}
				else
				{
					query->long_filename[i++] = query->current_entry_raw->ENTRY.STD.name[ret];
				}
			}
		}
		if (query->current_entry_raw->ENTRY.STD.name[8] != 0x20)
		{
			query->long_filename[i++] = '.';

			for (ret = 8; ret < 11; ret++)
			{
				if (query->current_entry_raw->ENTRY.STD.name[ret] != 0x20)
				{
					if (query->current_entry_raw->ENTRY.STD.reserved & FAT_LOWERCASE_EXTENSION)
					{
						query->long_filename[i++] = tolower(query->current_entry_raw->ENTRY.STD.name[ret]);
					}
					else
					{
						query->long_filename[i++] = query->current_entry_raw->ENTRY.STD.name[ret];
					}
				}
			}
		}
		query->long_filename[i] = 0x0;
	}
	#endif
	/*
	// return success
	*/
	return FAT_SUCCESS;
}

/*
// creates a FAT directory entry
*/
#if !defined(FAT_READ_ONLY)
uint16_t fat_create_directory_entry( 
	FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, 
	char* name, unsigned char attribs, uint32_t entry_cluster, FAT_DIRECTORY_ENTRY* new_entry) 
{

	uint16_t ret;
	int16_t char_index;
	uint16_t illegal_char;
	uint16_t entries_count = 0;
	uint32_t sector;
	uint32_t first_sector_of_cluster;
	uintptr_t last_entry_address;
	FAT_ENTRY fat;
	FAT_ENTRY last_fat;
	FAT_RAW_DIRECTORY_ENTRY* parent_entry;
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	FAT_RAW_DIRECTORY_ENTRY parent_entry_mem;
	uint16_t parent_entry_offset;
	#endif

	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	char no_of_lfn_entries_needed;
	char no_of_lfn_entries_found;
	char lfn_checksum;
	#endif
	
	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	unsigned char* buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	unsigned char* buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	#endif

	/*
	// get the length of the filename
	*/
	ret = strlen(name);
	/*
	// check that the character is a valid 8.3 filename, the
	// file is invalid if:
	//
	//	- name part is more than 8 chars (char_index > 8)
	//	- extension part is more than 3 (ret - char_index > 4)
	//	- it has more than one dot (indexof('.', name, char_index + 1) >= 0)
	*/	
	#if defined(FAT_DISABLE_LONG_FILENAMES)
	char_index = indexof('.', name, 0x0);
	if (char_index < 0 && ret > 8) 
	{
		return FAT_FILENAME_TOO_LONG;
	}
	if (char_index >= 0)
	{
		if (char_index > 8 || (ret - char_index) > 4) 
		{
			return FAT_FILENAME_TOO_LONG;
		}
		if (indexof('.', name, char_index + 1) >= 0) 
		{
			return FAT_INVALID_FILENAME;
		}
	}
	#else
	if (ret > 255)
	{
		return FAT_FILENAME_TOO_LONG;
	}
	#endif
	/*
	// all names are also invalid if they start or end with
	// a dot
	*/
	char_index = indexof('.', name, 0x0);

	if (char_index == 0 || char_index == (ret - 1))
	{
		return FAT_INVALID_FILENAME;
	}

	for (char_index = 0x0; char_index < ret; char_index++) 
	{
		/*
		// if the character is less than 0x20 with the
		// exception of 0x5 then the filename is illegal
		*/
		#if defined(FAT_DISABLE_LONG_FILENAMES)
		if (name[char_index] < 0x20)
		{
			return FAT_ILLEGAL_FILENAME;	
		}
		#else
		if (name[char_index] < 0x1F)
		{
			return FAT_ILLEGAL_FILENAME;	
		}
		#endif
		/*
		// compare the character with a table of illegal
		// characters, if a match is found then the filename
		// is illegal
		*/
		for (illegal_char = 0x0; illegal_char < ILLEGAL_CHARS_COUNT; illegal_char++)
		{
			if (name[char_index] == ILLEGAL_CHARS[illegal_char] && name[char_index] != '.')
			{
				return FAT_ILLEGAL_FILENAME;
			}
		}
	}
	/*
	// initialize the raw entry
	// todo: check if no other functions are initializing
	// new_entry and initialize the whole thing
	*/
	memset(&new_entry->raw, 0, sizeof(new_entry->raw));
	/*
	// attempt to format the filename provided
	// to the format required by the directory entry
	// and copy it to it's field
	*/
	ret = get_short_name_for_entry(new_entry->raw.ENTRY.STD.name, (unsigned char*) name, 0);
	/*
	// if the above operation failed then the filename
	// is invalid
	*/
	if (ret != FAT_SUCCESS && ret != FAT_LFN_GENERATED)
	{
		return FAT_INVALID_FILENAME;
	}
	/*
	// if this is going to be an lfn entry we need to make
	// sure that the short filename is available
	*/
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	if (ret == FAT_LFN_GENERATED)
	{
		FAT_QUERY_STATE query;
		uint16_t name_suffix = 0;
		char is_valid_entry;
		char name_suffix_str[6];
		unsigned char i, c;

		do
		{
			is_valid_entry = 1;

			memset(&query, 0, sizeof(query));
			query.buffer = query.buff;
			ret = fat_query_first_entry(volume, parent, 0, &query, 0);
			if (ret != FAT_SUCCESS)
			{
				return ret;
			}

			sprintf(name_suffix_str, "~%i", name_suffix);

			for (i = 0; i <  8 - (char) strlen(name_suffix_str); i++)
				if (new_entry->raw.ENTRY.STD.name[i] == 0x20)
					break;

			for (c = 0; c < (char) strlen(name_suffix_str); c++)
				new_entry->raw.ENTRY.STD.name[i++] = name_suffix_str[c];
			/*
			// loop through all entries in the parent directory
			// and if we find one with the same name as hours mark the name
			// as invalid
			*/
			while (*query.current_entry_raw->ENTRY.STD.name != 0)
			{
				if (memcmp(query.current_entry_raw->ENTRY.STD.name, new_entry->raw.ENTRY.STD.name, 11) == 0)
				{
					is_valid_entry = 0;
					break;
				}
				ret = fat_query_next_entry(volume, &query, 0, 0);
				if (ret != FAT_SUCCESS)
				{
					return ret;
				}
			}
			/*
			// if the filename is taken we need to compute a new one
			*/
			if (!is_valid_entry)
			{
				/*
				// create the filename suffix and append it after
				// the last char or replace the end of the filename
				// with it.
				*/
				sprintf(name_suffix_str, "~%i", name_suffix++);

				for (i = 0; i <  8 - (char) strlen(name_suffix_str); i++)
					if (new_entry->raw.ENTRY.STD.name[i] == 0x20)
						break;

				for (c = 0; c < (char) strlen(name_suffix_str); c++)
					new_entry->raw.ENTRY.STD.name[i++] = name_suffix_str[c];
			}
		}
		while (!is_valid_entry);
		/*
		// calculate the # of entries needed to store the lfn
		// including the actual entry
		*/
		no_of_lfn_entries_needed = ((strlen(name) + 12) / 13) + 1;
		no_of_lfn_entries_found = 0;
	}
	#endif
	/*
	// if the new entry is a directory and no cluster was supplied 
	// by the calling function then allocate a new cluster
	*/
	if (entry_cluster == 0 && (attribs & FAT_ATTR_DIRECTORY))
	{
		entry_cluster = fat_allocate_directory_cluster(volume, parent, &ret);
		if (ret != FAT_SUCCESS)
		{
			return ret;
		}
	}
	/*
	// set the entry attributes
	*/
	strcpy((char*) new_entry->name, name);
	new_entry->attributes = attribs;
	new_entry->size = 0x0;
	new_entry->raw.ENTRY.STD.attributes = attribs;
	new_entry->raw.ENTRY.STD.reserved = 0;
	new_entry->raw.ENTRY.STD.size = 0x0;
	new_entry->raw.ENTRY.STD.first_cluster_lo = LO16(entry_cluster);
	new_entry->raw.ENTRY.STD.first_cluster_hi = HI16(entry_cluster);	
	new_entry->raw.ENTRY.STD.create_time_tenth = 0x0;
	new_entry->raw.ENTRY.STD.create_date = rtc_get_fat_date();
	new_entry->raw.ENTRY.STD.create_time = rtc_get_fat_time();
	new_entry->raw.ENTRY.STD.modify_date = new_entry->raw.ENTRY.STD.create_date;
	new_entry->raw.ENTRY.STD.modify_time = new_entry->raw.ENTRY.STD.create_time;
	new_entry->raw.ENTRY.STD.access_date = new_entry->raw.ENTRY.STD.create_date;
	new_entry->create_time = fat_decode_date_time(new_entry->raw.ENTRY.STD.create_date, new_entry->raw.ENTRY.STD.create_time);
	new_entry->modify_time = fat_decode_date_time(new_entry->raw.ENTRY.STD.modify_date, new_entry->raw.ENTRY.STD.modify_time);
	new_entry->access_time = fat_decode_date_time(new_entry->raw.ENTRY.STD.access_date, 0);

	/*
	// there's no fat entry that points to the 1st cluster of
	// a directory's cluster chain but we'll create a
	// fake fat entry from the 1st cluster data on the
	// directory entry so that we can handle the 1st
	// cluster with the same code as all other clusters
	// in the chain
	*/
	if (parent && (parent->ENTRY.STD.first_cluster_lo != 0x0 || parent->ENTRY.STD.first_cluster_hi != 0x0)) 
	{
		/*
		// read the low word of the cluster address
		// and read the high word of the 1st cluster address
		// ONLY if the file system type is FAT32
		*/
		((uint16_t*) &fat)[INT32_WORD0] = parent->ENTRY.STD.first_cluster_lo;
		((uint16_t*) &fat)[INT32_WORD1] = (volume->fs_type == FAT_FS_TYPE_FAT32) ? parent->ENTRY.STD.first_cluster_hi : 0x0;
	}
	/*
	// if no parent was specified then we create
	// the fake fat entry from the root directory's
	// cluster address found on the volume structure
	*/
	else 
	{
		if (volume->fs_type == FAT_FS_TYPE_FAT32)
		{
			fat = volume->root_cluster;
		}
		else 
		{
			fat = last_fat = 0x0;
			first_sector_of_cluster =
				volume->no_of_reserved_sectors + (volume->no_of_fat_tables * volume->fat_size);
		}
	}
	/*
	// set parent_entry to point to the memory we've
	// allocated for it (if needed).
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	parent_entry = &parent_entry_mem;
	#endif
	/*
	// acquire a lock on the buffer
	*/
	#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
	ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
	#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
	ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
	#endif
	/*
	// mark the cached sector as unknown
	*/
	FAT_SET_LOADED_SECTOR(volume, FAT_UNKNOWN_SECTOR);
	/*
	// for each cluster allocated to the parent
	// directory entry
	*/
	do 
	{
		/*
		// calculate the address of the 1st sector
		// of the cluster - skip this step if ret equals
		// 1, this means that this is the 1st sector of the
		// root entry which doesn't start at the beggining 
		// of the cluster
		*/
		if (fat != 0x0)
		{
			first_sector_of_cluster = FIRST_SECTOR_OF_CLUSTER(volume, fat);
		}
		/*
		// set the current sector to the first
		// sector of the cluster
		*/
		sector = first_sector_of_cluster;
		/*
		// calculate the address of the last directory
		// entry on a sector when the sector is loaded
		// into sec_buff
		*/	
		last_entry_address = 
			((uintptr_t) buffer + volume->no_of_bytes_per_serctor) - 0x20;
		/*
		// for each sector in the cluster
		*/
		while (fat == 0 || sector < (first_sector_of_cluster + volume->no_of_sectors_per_cluster)) 
		{
			/*
			// read the current sector to RAM
			*/
			ret = volume->device->read_sector(volume->device->driver, sector, buffer);
			if (ret != STORAGE_SUCCESS)
			{
				#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
				LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
				#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
				LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
				#endif
				return ret;
			}
			/*
			// set the parent entry pointer to the 1st
			// entry of the current sector
			*/
			#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
			fat_read_raw_directory_entry(parent_entry, buffer);
			parent_entry_offset = 0;
			#else
			parent_entry = (FAT_RAW_DIRECTORY_ENTRY*) buffer;
			#endif
			/*
			// for each directory entry in the sector...
			*/
			#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
			while (parent_entry_offset < volume->no_of_bytes_per_serctor)
			#else
			while ((uintptr_t) parent_entry <= last_entry_address) 
			#endif
			{
				/*
				// make sure we don't exceed the limit of 0xFFFF entries
				// per directory
				*/
				#if defined(FAT_DISABLE_LONG_FILENAMES)
				if (entries_count == 0xFFFF)
				#else
				if ((entries_count + no_of_lfn_entries_needed) == 0xFFFF)
				#endif
				{
					#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
					LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
					#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
					LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
					#endif
					return FAT_DIRECTORY_LIMIT_EXCEEDED;
				}
				/*
				// increase the count of directory entries
				*/
				entries_count++;
				/*
				// if the directory entry is free
				*/
				if (IS_FREE_DIRECTORY_ENTRY(parent_entry)) 
				{
					#if defined(FAT_DISABLE_LONG_FILENAMES)
					/*
					// update the FAT entry with the data from
					// the newly created entry
					*/
					#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
					fat_write_raw_directory_entry(parent_entry, buffer + parent_entry_offset);
					new_entry->sector_addr = sector;
					new_entry->sector_offset = parent_entry_offset;
					#else
					*parent_entry = new_entry->raw;
					new_entry->sector_addr = sector;
					new_entry->sector_offset = (uintptr_t) parent_entry - (uintptr_t) buffer;
					#endif
					if ((ret = volume->device->write_sector(volume->device->driver, sector, buffer)) != STORAGE_SUCCESS)
					{
						#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
						LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
						#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
						LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
						#endif
						return FAT_CANNOT_WRITE_MEDIA;
					}
					/*
					// release the lock on the buffer
					*/
					#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
					LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
					#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
					LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
					#endif
					/*
					// store the sector and offset of the entry and go
					*/
					return FAT_SUCCESS;
					#else
					/*
					// we've found a free entry
					*/
					no_of_lfn_entries_found++;
					/*
					// if this is the last directory entry or if we've
					// found all the entries that we need then let's get
					// ready to write them
					*/
					if (IS_LAST_DIRECTORY_ENTRY(parent_entry) || no_of_lfn_entries_found == no_of_lfn_entries_needed)
					{
						/*
						// if there where any free entries before this
						// one then we need to rewind a bit
						*/
						while (no_of_lfn_entries_found-- > 1)
						{
							#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
							if (parent_entry_offset)
							#else
							if ((uintptr_t) parent_entry > (uintptr_t) buffer)
							#endif
							{
								#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
								parent_entry_offset -= 0x20;
								fat_read_raw_directory_entry(parent_entry, buffer + parent_entry_offset);
								#else
								parent_entry--;
								#endif
							}
							else
							{
								/*
								// if the last entry is on the same cluster we
								// can just decrease the sector number, otherwise we
								// need to get the sector address for the last cluster
								*/
								if (sector > first_sector_of_cluster)
								{
									sector--;
								}
								else
								{
									if (last_fat == 0)
									{
										first_sector_of_cluster =
											volume->no_of_reserved_sectors + (volume->no_of_fat_tables * volume->fat_size);
									}
									else
									{
										fat = last_fat;
										first_sector_of_cluster = FIRST_SECTOR_OF_CLUSTER(volume, fat);
									}
									sector = first_sector_of_cluster + volume->no_of_sectors_per_cluster;
								}
								/*
								// read the last sector to the cache, calculate the last
								// entry address and set our pointer to it
								*/
								ret = volume->device->read_sector(volume->device->driver, sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
									LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
									#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
									LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
									#endif
									return ret;
								}
								last_entry_address = ((uintptr_t) buffer + volume->no_of_bytes_per_serctor) - 0x20;
								#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
								parent_entry_offset = volume->no_of_bytes_per_serctor - 0x20;
								fat_read_raw_directory_entry(parent_entry, buffer + parent_entry_offset);
								#else
								parent_entry = (FAT_RAW_DIRECTORY_ENTRY*) last_entry_address;
								#endif
							}
						}
						/*
						// compute the checksum for this entry
						*/
						lfn_checksum = fat_long_entry_checksum((unsigned char*)new_entry->raw.ENTRY.STD.name);
						/*
						// now we can start writting
						*/
						no_of_lfn_entries_found = no_of_lfn_entries_needed;
						while (no_of_lfn_entries_found--)
						{
							if (no_of_lfn_entries_found)
							{
								uint16_t i, c;
								/*
								// set the required fields for this entry
								*/
								parent_entry->ENTRY.LFN.lfn_sequence = (unsigned char) no_of_lfn_entries_found;
								parent_entry->ENTRY.LFN.lfn_checksum = lfn_checksum;
								parent_entry->ENTRY.STD.attributes = FAT_ATTR_LONG_NAME;
								parent_entry->ENTRY.LFN.lfn_first_cluster = 0;
								parent_entry->ENTRY.LFN.lfn_type = 0;
								/*
								// mark entry as the 1st entry if it is so
								*/
								if (no_of_lfn_entries_found == no_of_lfn_entries_needed - 1)
									parent_entry->ENTRY.LFN.lfn_sequence = parent_entry->ENTRY.LFN.lfn_sequence | FAT_FIRST_LFN_ENTRY;
								/*
								// copy the lfn chars
								*/
								c = strlen(name);
								i = ((no_of_lfn_entries_found - 1) * 13);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x0] = LO8((i + 0x0 > c) ? 0xFFFF : (uint16_t) name[i + 0x0]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x1] = HI8((i + 0x0 > c) ? 0xFFFF : (uint16_t) name[i + 0x0]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x2] = LO8((i + 0x1 > c) ? 0xFFFF : (uint16_t) name[i + 0x1]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x3] = HI8((i + 0x1 > c) ? 0xFFFF : (uint16_t) name[i + 0x1]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x4] = LO8((i + 0x2 > c) ? 0xFFFF : (uint16_t) name[i + 0x2]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x5] = HI8((i + 0x2 > c) ? 0xFFFF : (uint16_t) name[i + 0x2]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x6] = LO8((i + 0x3 > c) ? 0xFFFF : (uint16_t) name[i + 0x3]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x7] = HI8((i + 0x3 > c) ? 0xFFFF : (uint16_t) name[i + 0x3]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x8] = LO8((i + 0x4 > c) ? 0xFFFF : (uint16_t) name[i + 0x4]);
								parent_entry->ENTRY.LFN.lfn_chars_1[0x9] = HI8((i + 0x4 > c) ? 0xFFFF : (uint16_t) name[i + 0x4]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x0] = LO8((i + 0x5 > c) ? 0xFFFF : (uint16_t) name[i + 0x5]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x1] = HI8((i + 0x5 > c) ? 0xFFFF : (uint16_t) name[i + 0x5]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x2] = LO8((i + 0x6 > c) ? 0xFFFF : (uint16_t) name[i + 0x6]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x3] = HI8((i + 0x6 > c) ? 0xFFFF : (uint16_t) name[i + 0x6]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x4] = LO8((i + 0x7 > c) ? 0xFFFF : (uint16_t) name[i + 0x7]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x5] = HI8((i + 0x7 > c) ? 0xFFFF : (uint16_t) name[i + 0x7]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x6] = LO8((i + 0x8 > c) ? 0xFFFF : (uint16_t) name[i + 0x8]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x7] = HI8((i + 0x8 > c) ? 0xFFFF : (uint16_t) name[i + 0x8]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x8] = LO8((i + 0x9 > c) ? 0xFFFF : (uint16_t) name[i + 0x9]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0x9] = HI8((i + 0x9 > c) ? 0xFFFF : (uint16_t) name[i + 0x9]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0xA] = LO8((i + 0xA > c) ? 0xFFFF : (uint16_t) name[i + 0xA]);
								parent_entry->ENTRY.LFN.lfn_chars_2[0xB] = HI8((i + 0xA > c) ? 0xFFFF : (uint16_t) name[i + 0xA]);
								parent_entry->ENTRY.LFN.lfn_chars_3[0x0] = LO8((i + 0xB > c) ? 0xFFFF : (uint16_t) name[i + 0xB]);
								parent_entry->ENTRY.LFN.lfn_chars_3[0x1] = HI8((i + 0xB > c) ? 0xFFFF : (uint16_t) name[i + 0xB]);
								parent_entry->ENTRY.LFN.lfn_chars_3[0x2] = LO8((i + 0xC > c) ? 0xFFFF : (uint16_t) name[i + 0xC]);
								parent_entry->ENTRY.LFN.lfn_chars_3[0x3] = HI8((i + 0xC > c) ? 0xFFFF : (uint16_t) name[i + 0xC]);
								/*
								// write updated entry to buffer
								*/
								#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
								fat_write_raw_directory_entry(parent_entry, buffer + parent_entry_offset);
								#endif
								/*
								// continue to next entry
								*/
								#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
								if (parent_entry_offset < volume->no_of_bytes_per_serctor - 0x20)
								#else
								if ((uintptr_t) parent_entry < (uintptr_t) last_entry_address)
								#endif
								{
									#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
									parent_entry_offset += 0x20;
									fat_read_raw_directory_entry(parent_entry, buffer + parent_entry_offset);
									#else
									parent_entry++;
									#endif
								}
								else
								{
									/*
									// flush this sector to the storage device and
									// load the next sector
									*/
									ret = volume->device->write_sector(volume->device->driver, sector, buffer);
									if (ret != STORAGE_SUCCESS)
									{
										#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
										LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
										#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
										LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
										#endif
										return FAT_CANNOT_WRITE_MEDIA;
									}

									if (fat == 0 || sector < first_sector_of_cluster + volume->no_of_sectors_per_cluster - 1)
									{
										sector++;
										/*
										// make sure that we don't overflow the root directory
										// on FAT12/16 volumes
										*/
										if (!fat)
										{
											if (sector > first_sector_of_cluster + volume->root_directory_sectors)
											{
												#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
												LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
												#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
												LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
												#endif
												return FAT_ROOT_DIRECTORY_LIMIT_EXCEEDED;
											}
										}
									}
									else
									{
										/*
										// release the lock on the buffer
										*/
										#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
										LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
										#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
										LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
										#endif
										/*
										// get the next cluster, we'll remember the last one so
										// we can update it bellow if it's the eof cluster
										*/
										last_fat = fat;
										ret = fat_get_cluster_entry( volume, fat, &fat );
										if (ret != FAT_SUCCESS)
										{
											return ret;
										}
										/*
										// if this is the end of the FAT chain allocate
										// a new cluster to this folder and continue
										*/
										if (fat_is_eof_entry(volume, fat))
										{
											FAT_ENTRY newfat = fat_allocate_data_cluster(volume, 1, 1, &ret);
											if (ret != FAT_SUCCESS) 
											{
												return ret;
											}

											ret = fat_set_cluster_entry(volume, last_fat, newfat);
											if (ret != FAT_SUCCESS) 
											{
												return ret;
											}
											fat = newfat;
										}
										/*
										// re-acquire the lock on the buffer
										*/
										#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
										ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
										#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
										ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
										#endif
										/*
										// mark the loaded sector as unknown
										*/
										FAT_SET_LOADED_SECTOR(volume, FAT_UNKNOWN_SECTOR);
										/*
										// continue working on the new cluster
										*/
										sector = FIRST_SECTOR_OF_CLUSTER(volume, fat);
									}
									/*
									// load the next sector
									*/
									ret = volume->device->read_sector(volume->device->driver, sector, buffer);
									if (ret != STORAGE_SUCCESS)
									{
										#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
										LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
										#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
										LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
										#endif
										return ret;
									}
									#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
									fat_read_raw_directory_entry(parent_entry, buffer);
									parent_entry_offset = 0;
									#else
									parent_entry = (FAT_RAW_DIRECTORY_ENTRY*) buffer;
									#endif
								}
							}
							else
							{
								*parent_entry = new_entry->raw;
								#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
								fat_write_raw_directory_entry(parent_entry, buffer + parent_entry_offset);
								#endif
							}
						}
						/*
						// flush this sector to the storage device and
						// load the next sector
						*/
						ret = volume->device->write_sector(volume->device->driver, sector, buffer);
						if (ret != STORAGE_SUCCESS)
						{
							#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
							LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
							#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
							LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
							#endif
							return FAT_CANNOT_WRITE_MEDIA;
						}
						new_entry->sector_addr = sector;
						#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
						new_entry->sector_offset = parent_entry_offset;
						#else
						new_entry->sector_offset = (uintptr_t) parent_entry - (uintptr_t) buffer;
						#endif
						/*
						// release the lock on the buffer
						*/
						#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
						LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
						#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
						LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
						#endif
						/*
						// we're done!!!!!
						*/
						return FAT_SUCCESS;
					}
					#endif
				}
				#if !defined(FAT_DISABLE_LONG_FILENAMES)
				else
				{
					no_of_lfn_entries_found = 0;
				}
				#endif
				/*
				// move the parent entry pointer to
				// the next entry in the sector
				*/
				#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
				/* fat_write_raw_directory_entry(parent_entry, buffer + parent_entry_offset); */
				parent_entry_offset += 0x20;
				fat_read_raw_directory_entry(parent_entry, buffer + parent_entry_offset);
				#else
				parent_entry++;
				#endif
			}
			/*
			// move to the next sector in the cluster
			*/
			sector++;
			/*
			// make sure that we don't overflow the root directory
			// on FAT12/16 volumes
			*/
			if (!fat)
			{
				if (sector > first_sector_of_cluster + volume->root_directory_sectors)
				{
					#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
					LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
					#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
					LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
					#endif
					return FAT_ROOT_DIRECTORY_LIMIT_EXCEEDED;
				}
			}
		}
		/*
		// the following functions need the buffer so we must
		// release it's lock before calling them
		*/
		#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
		LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock);
		#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
		LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock);
		#endif
		/*
		// get the next cluster, we'll remember the last one in
		// case we need to rewind to write lfn entries
		*/
		last_fat = fat;
		ret = fat_get_cluster_entry( volume, fat, &fat );
		if (ret != FAT_SUCCESS)
			return ret;
		/*
		// if this is the end of the FAT chain allocate
		// a new cluster to this folder and continue
		*/
		if (fat_is_eof_entry(volume, fat))
		{
			FAT_ENTRY newfat;
			/*
			// allocate the cluster
			*/
			newfat = fat_allocate_data_cluster(volume, 1, 1, &ret);
			if (ret != FAT_SUCCESS) 
				return ret;
			/*
			// link it to the cluster chain
			*/
			ret = fat_set_cluster_entry(volume, last_fat, newfat);
			if (ret != FAT_SUCCESS) 
				return ret;
			fat = newfat;
		}
		/*
		// re-acquire the lock on the buffer
		*/
		#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
		ENTER_CRITICAL_SECTION(volume->sector_buffer_lock);
		#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
		ENTER_CRITICAL_SECTION(fat_shared_buffer_lock);
		#endif
		/*
		// mark the loaded sector as unknown
		*/
		FAT_SET_LOADED_SECTOR(volume, FAT_UNKNOWN_SECTOR);
	}
	while (1);
}
#endif

/*
// converts a 8.3 filename from the internal
// filesystem format to the user friendly convention
*/
void INLINE fat_get_short_name_from_entry( unsigned char* dest, const unsigned char* src ) 
{
	/*
	// copy the 1st character - 0xE5 is used on the
	// 1st character of the entry to indicate an unused
	// entry but it is also a valid KANJI lead byte for the
	// character set used in Japan. The special value 0x5 is 
	// used so that this special file name case for Japan 
	// can be handled properly and not cause the FAT code
	// to think that the entry is free.
	*/
	if ( *src != 0x5 )
		*dest++ = *src++;
	else *dest++ = 0xE5;
	/*
	// if there's a second character...
	*/
	if ( *src != 0x20 ) 
	{
		*dest++ = *src++;
		if ( *src != 0x20 ) 
		{
			*dest++ = *src++;
			if ( *src != 0x20 ) 
			{
				*dest++ = *src++;
				if ( *src != 0x20 ) 
				{
					*dest++ = *src++;
					if ( *src != 0x20 ) 
					{
						*dest++ = *src++;
						if ( *src != 0x20 ) 
						{
							*dest++ = *src++;
							if ( *src != 0x20 ) 
							{
								*dest++ = *src++;
							}
							else src++;
						}
						else src += 0x2;
					}
					else src += 0x3;
				}
				else src += 0x4;
			}
			else src += 0x5;
		}
		else src += 0x6;
	}
	else src += 0x7;	
	/*
	// if there's an extension append it to
	// the output
	*/		
	if ( *src != 0x20 ) 
	{
		*dest++ = '.';
		*dest++ = *src++;
		if ( *src != 0x20 ) 
		{
			*dest++ = *src++;
			if ( *src != 0x20 )
			{
				*dest++ = *src;
			}
		}
	}
	*dest = 0x0;
}	

/*
// converts the filename to it's Unicode UTF16 representation
*/
#if !defined(FAT_DISABLE_LONG_FILENAMES)
char INLINE get_long_name_for_entry(uint16_t* dst, unsigned char* src)
{
	register int i;
	for (i = 0; i < (int) strlen((char*) src); i++)
	{
		dst[i] = (uint16_t) src[i];
	}
	dst[i] = 0x0;
	/*
	// todo: check that this is a valid filename
	// and that it only uses ASCII chars since we don't
	// support unicode at this time
	*/
	return FAT_SUCCESS;
}
#endif


/*
// compares two short names after they
// have been formatted by get_short_name_for_entry
// returns 1 if both name are equal and 0 otherwise
*/
char INLINE fat_compare_short_name( unsigned char* name1, unsigned char* name2 ) 
{
	return memcmp(name1, name2, 11) == 0;
}

/*
// performs an ASCII comparison on two UTF16 strings
*/
#if !defined(FAT_DISABLE_LONG_FILENAMES)
char INLINE fat_compare_long_name( uint16_t* name1, uint16_t* name2 )
{	
	register short i;
	for (i = 0; i < 256; i++)
	{
		if (toupper((char)name1[i]) != toupper((char)name2[i]))
			return 0;
		if ((char)name1[i] == 0x0)
			return 1;
	}
	return 1;
}
#endif

/*
// converts an 8.3 filename to the format required
// by the FAT directory entry structure
*/
uint16_t get_short_name_for_entry(unsigned char* dest, unsigned char* src, char lfn_disabled) 
{
	
	char tmp[13];
	char has_uppercase = 0;
	uint16_t dot_index;
	uint16_t length;
	uint16_t i;
	/*
	// check that the name is actually a long filename
	// before processing it as such
	*/
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	if (!lfn_disabled)
	{
		unsigned char c;
		char is_lfn = 0;
		length = strlen((char*) src);
		dot_index = indexof('.', (char*)src, 0x0);
		/*
		// if the file hs no extension and is longer than 8 chars
		// or if the name part has more than 8 chars or the extension more than 8
		// or if it has more than one dot then we need to handle it as a lfn
		*/
		if (dot_index < 0 && length > 8) is_lfn = 1;
		if (dot_index >= 0) 
		{
			if (dot_index > 7 || (length - dot_index/*, 0*/) > 4) is_lfn = 1;
			if (dot_index >= 0) if (indexof('.', (char*)src, 1) >= 0) is_lfn = 1;
		}
		else
		{
			if (length > 8) is_lfn = 1;
		}
		/*
		// if it has spaces or lowercase letters we must also 
		// handle it as a long filename
		*/
		if (!is_lfn)
		{
			for (i = 0; i < length; i++)
			{
				if (src[i] == 0x20 || src[i] != toupper(src[i]))
				{
					is_lfn = 1;
					break;
				}
			}
		}
		if (is_lfn)
		{
			/*
			// first we find the location of the LAST dot
			*/
			dot_index = length;
			for (i = length - 1; i; i--)
			{
				if (src[i] == '.')
				{
					dot_index = i;
					break;
				}
			}
			/*
			// now we copy the first 8 chars of the filename
			// excluding dots and spaces and we pad it with
			// spaces
			*/
			c = 0;
			for (i = 0; i < 8; i++)
			{
				while (c < dot_index)
				{
					if (src[c] == 0x20 || src[c] == '.') 
					{
						c++;
					}
					else 
					{
						break;
					}
				}
				if (c < dot_index)
				{
					tmp[i] = toupper(src[c++]);
				}
				else
				{
					tmp[i] = 0x20;
				}
			}
			/*
			// do the same for the extension
			*/
			c = dot_index + 1;
			for (i = 8; i < 11; i++)
			{
				while (c < length)
				{
					if (src[c] == 0x20 || src[c] == '.') 
					{
						c++;
					}
					else
					{
						break;
					}
				}
				if (c < length)
				{
					tmp[i] = toupper(src[c++]);
				}
				else
				{
					tmp[i] = 0x20;
				}
			}
			/*
			// now we copy it to the callers buffer and we're done
			*/
			for (i = 0; i < 11; i++)
				*dest++ = tmp[i];
			/*
			// return special code so the caller knows
			// to store the long name
			*/
			return FAT_LFN_GENERATED;
		}
	}
	#endif
	/*
	// trim-off spaces - if the result is
	// greater than 12 it will return an empty
	// string
	*/
	strtrim( tmp, (char*)src, 12 );
	/*
	// if the name length was invalid return
	// error code
	*/
	if ( *tmp == 0 || strlen(tmp) > 12) 
		return FAT_INVALID_FILENAME;
	/*
	// find the location of the dot
	*/
	dot_index = ( uintptr_t ) strchr( tmp, ( int ) '.' );
	
	/*
	// strchr gave us the address of the dot, we now
	// convert it to a 1-based index
	*/
	if ( dot_index )
		dot_index -= ( uintptr_t ) tmp - 0x1;
	/*
	// get the length of the input string
	*/
	length = strlen( tmp );
	/*
	// check that this is a valid 8.3 filename
	*/	
	if ( ( length > 0x9 &&
		( dot_index == 0x0 || ( dot_index ) > 0x9 ) ) ||
		( dot_index  > 0x0 && ( length - dot_index ) > 0x5 ) )
		return FAT_INVALID_FILENAME;
	/*
	// copy the 1st part of the filename to the
	// destination buffer
	*/
	for (i = 0x0; i < 0x8; i++) 
	{
		if (dot_index == 0x0) 
		{
			if (i < length) 
			{
				if (lfn_disabled && (tmp[i] != toupper(tmp[i])))
					has_uppercase = 1;

				*dest++ = toupper(tmp[i]);
			}
			else 
			{
				*dest++ = 0x20;
			}
		}
		else 
		{
			if (i < dot_index - 0x1) 
			{	
				if (lfn_disabled && (tmp[i] != toupper(tmp[i])))
					has_uppercase = 1;

				*dest++ = toupper(tmp[i]);
			}
			else 
			{
				*dest++ = 0x20;
			}
		}
	}
	/*
	// if there's not extension fill the extension
	// characters with spaces
	*/
	if ( dot_index == 0x0 ) 		
	{
		for ( i = 0x0; i < 0x3; i++ )
			*dest++ = 0x20;
	}
	/*
	// if there is an extension...
	*/
	else 
	{
		/*
		// copy the extension characters to the
		// destination buffer
		*/
		for (i = dot_index; i < dot_index + 0x3; i++)	
		{
			if ( i < length ) 
			{
				if (lfn_disabled && (tmp[i] != toupper(tmp[i])))
					has_uppercase = 1;
				*dest++ = toupper(tmp[i]);
			}
			else 
			{
				*dest++ = 0x20;
			}
		}
	}
	/*
	// return success code
	*/
	return has_uppercase ? FAT_INVALID_FILENAME : FAT_SUCCESS;	
}	

/*
// computes the short filename checksum
*/
#if !defined(FAT_DISABLE_LONG_FILENAMES)
unsigned char fat_long_entry_checksum( unsigned char* filename ) 
{
	uint16_t len;
	unsigned char sum = 0;
	for (len = 11; len != 0; len--) 
	{
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *filename++;
	}
	return sum;
}
#endif

INLINE int indexof(char chr, char* str, int index)
{
	int i = 0;
	str = str + index;
	
	do
	{
		if (str[i] == chr)
			return i;
		i++;
	}
	while (str[i]);

	return -1;
}

#if !defined(FAT_READ_ONLY)
uint16_t rtc_get_fat_date() 
{ 	
	#if defined(FAT_USE_SYSTEM_TIME)
	time_t now;
	struct tm* timeinfo;
	time(&now);
	timeinfo = localtime(&now);
	return FAT_ENCODE_DATE(timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_year + 1900);
	#else
	if (timekeeper.fat_get_system_time)
	{
		time_t now = timekeeper.fat_get_system_time();
		struct tm* timeinfo = localtime(&now);
		return FAT_ENCODE_DATE(timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_year + 1900);
	}
	else
	{
		return FAT_ENCODE_DATE(8, 2, 1985); 
	}
	#endif
}
#endif

#if !defined(FAT_READ_ONLY)
uint16_t rtc_get_fat_time() 
{ 
	#if defined(FAT_USE_SYSTEM_TIME)
	time_t now;
	struct tm* timeinfo;
	time(&now);
	timeinfo = localtime(&now);
	return FAT_ENCODE_TIME(timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	#else
	if (timekeeper.fat_get_system_time)
	{
		time_t now;
		struct tm* timeinfo;
		now = timekeeper.fat_get_system_time();
		timeinfo = localtime(&now);
		return FAT_ENCODE_TIME(timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	}
	else
	{
		return FAT_ENCODE_TIME(12, 5, 35);
	}
	#endif
}
#endif

time_t fat_decode_date_time(uint16_t date, uint16_t time) { 
	/*
	//YYYYYYYMMMMDDDDD
	//0000000000011111
	//0000000111100000
	//1111111000000000	
	//HHHHHMMMMMMSSSSS
	//0000000000011111
	//0000011111100000
	//1111100000000000	
	//struct tm datetime;
	//time_t now;
	*/
	struct tm tm;
	tm.tm_year = ((date) >> 9) + 80;
	tm.tm_mon = (((date) & 0x1E0) >> 5) - 1;
	tm.tm_mday = (date) & 0x1F;
	tm.tm_hour = (time) >> 11;
	tm.tm_min = ((time) & 0x7E0) >> 5;
	tm.tm_sec = ((time) & 0x1F) << 1;
	return mktime(&tm);
}

/*
// treams leading and trailing spaces. If the result
// exceeds the max length the destination will be set
// to an empty string
// todo: clean it up a bit
*/
void strtrim(char* dest, char* src, size_t max ) {
	
	uint32_t max_length;
	uint32_t lead_spaces = 0x0;
	uint32_t last_char = 0x0;
	uint32_t i;
	char* dst = dest;
	
	max_length = strlen( src );
	/*
	// count the lead spaces
	*/
	for ( i = 0; i < max_length && src[ i ] == 0x20; i++ )
		lead_spaces++;
	/*
	// if the whole string is full of spaces
	// return an empty string
	*/
	if ( max_length == lead_spaces ) {
		*dest = 0x0;
		return;	
	}
	/*
	// calculate the index of the last non-space
	// character
	*/
	for ( last_char = max_length - 1; 
		last_char > 0 && ( src[ last_char ] == 0x20 ); last_char-- );

	/*
	// copy the non-space characters to the
	// destination buffer
	*/
	for ( i = lead_spaces; i <= last_char; i++ )
	{
		*dest++ = src[ i ];
		if (!max--)
		{
			*dst = 0x0;
			return;
		}
	}
	/*
	// set the null terminator
	*/	
	*dest = 0x0;
}

void fat_parse_path(char* path, char* path_part, char** filename_part)
{
	*filename_part = path + strlen(path);
	while (*--(*filename_part) != '\\' && (*filename_part) != path);
	
	while (path != *filename_part)
		*path_part++ = *path++;
	*path_part = 0;
	(*filename_part)++;
}

#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)

void fat_read_fsinfo(FAT_FSINFO* fsinfo, unsigned char* buffer)
{
	uint16_t i;
	((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE3] = *buffer++;
	fsinfo->Reserved2[0] = *buffer++;
	fsinfo->Reserved2[1] = *buffer++;
	fsinfo->Reserved2[2] = *buffer++;
	fsinfo->Reserved2[3] = *buffer++;
	fsinfo->Reserved2[4] = *buffer++;
	fsinfo->Reserved2[5] = *buffer++;
	fsinfo->Reserved2[6] = *buffer++;
	fsinfo->Reserved2[7] = *buffer++;
	fsinfo->Reserved2[8] = *buffer++;
	fsinfo->Reserved2[9] = *buffer++;
	fsinfo->Reserved2[10] = *buffer++;
	fsinfo->Reserved2[11] = *buffer++;
	((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE3] = *buffer++;
	((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE3] = *buffer++;
	((unsigned char*) &fsinfo->StructSig)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &fsinfo->StructSig)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &fsinfo->StructSig)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &fsinfo->StructSig)[INT32_BYTE3] = *buffer++;

	for (i = 0; i < 480; i++)
	{
		fsinfo->Reserved1[i] = *buffer++;
	}

	((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE3] = *buffer++;
}

void fat_write_fsinfo(FAT_FSINFO* fsinfo, unsigned char* buffer)
{
	uint16_t i;
	*buffer++ = ((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &fsinfo->TrailSig)[INT32_BYTE3];
	*buffer++ = fsinfo->Reserved2[0];
	*buffer++ = fsinfo->Reserved2[1];
	*buffer++ = fsinfo->Reserved2[2];
	*buffer++ = fsinfo->Reserved2[3];
	*buffer++ = fsinfo->Reserved2[4];
	*buffer++ = fsinfo->Reserved2[5];
	*buffer++ = fsinfo->Reserved2[6];
	*buffer++ = fsinfo->Reserved2[7];
	*buffer++ = fsinfo->Reserved2[8];
	*buffer++ = fsinfo->Reserved2[9];
	*buffer++ = fsinfo->Reserved2[10];
	*buffer++ = fsinfo->Reserved2[11];
	*buffer++ = ((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &fsinfo->Nxt_Free)[INT32_BYTE3];
	*buffer++ = ((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &fsinfo->Free_Count)[INT32_BYTE3];
	*buffer++ = ((unsigned char*) &fsinfo->StructSig)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &fsinfo->StructSig)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &fsinfo->StructSig)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &fsinfo->StructSig)[INT32_BYTE3];

	for (i = 0; i < 480; i++)
	{
		*buffer++ = fsinfo->Reserved1[i];
	}

	*buffer++ = ((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &fsinfo->LeadSig)[INT32_BYTE3];
}

void fat_read_partition_entry(FAT_PARTITION_ENTRY* entry, unsigned char* buffer)
{
	entry->status = *buffer++;
	entry->chs_first_sector[0] = *buffer++;
	entry->chs_first_sector[1] = *buffer++;
	entry->chs_first_sector[2] = *buffer++;
	entry->partition_type = *buffer++;
	entry->chs_last_sector[0] = *buffer++;
	entry->chs_last_sector[1] = *buffer++;
	entry->chs_last_sector[2] = *buffer++;
	((unsigned char*) &entry->lba_first_sector)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &entry->lba_first_sector)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &entry->lba_first_sector)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &entry->lba_first_sector)[INT32_BYTE3] = *buffer++;
	((unsigned char*) &entry->total_sectors)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &entry->total_sectors)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &entry->total_sectors)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &entry->total_sectors)[INT32_BYTE3] = *buffer++;
}

/*
// reads bpb from buffer
*/
void fat_read_bpb(FAT_BPB* bpb, unsigned char* buffer)
{
	uint32_t no_of_sectors;
	uint32_t no_of_clusters;
	unsigned char fs_type;
	/*
	// read standard bpb
	*/
	bpb->BS_jmpBoot[0] = *buffer++;
	bpb->BS_jmpBoot[1] = *buffer++;
	bpb->BS_jmpBoot[2] = *buffer++;
	bpb->BS_OEMName[0] = *buffer++;
	bpb->BS_OEMName[1] = *buffer++;
	bpb->BS_OEMName[2] = *buffer++;
	bpb->BS_OEMName[3] = *buffer++;
	bpb->BS_OEMName[4] = *buffer++;
	bpb->BS_OEMName[5] = *buffer++;
	bpb->BS_OEMName[6] = *buffer++;
	bpb->BS_OEMName[7] = *buffer++;
	((unsigned char*) &bpb->BPB_BytsPerSec)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_BytsPerSec)[INT16_BYTE1] = *buffer++;
	bpb->BPB_SecPerClus = *buffer++;
	((unsigned char*) &bpb->BPB_RsvdSecCnt)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_RsvdSecCnt)[INT16_BYTE1] = *buffer++;
	bpb->BPB_NumFATs = *buffer++;
	((unsigned char*) &bpb->BPB_RootEntCnt)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_RootEntCnt)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &bpb->BPB_TotSec16)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_TotSec16)[INT16_BYTE1] = *buffer++;
	bpb->BPB_Media = *buffer++;
	((unsigned char*) &bpb->BPB_FATSz16)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_FATSz16)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &bpb->BPB_SecPerTrk)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_SecPerTrk)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &bpb->BPB_NumHeads)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_NumHeads)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE3] = *buffer++;
	((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE3] = *buffer++;
	/*
	// determine filesystem type
	*/
	no_of_sectors = (bpb->BPB_TotSec16) ? bpb->BPB_TotSec16 : bpb->BPB_TotSec32;	
	no_of_clusters = no_of_sectors / bpb->BPB_SecPerClus;
	fs_type = (no_of_clusters < 4085) ? FAT_FS_TYPE_FAT12 :
		(no_of_clusters < 65525) ? FAT_FS_TYPE_FAT16 : FAT_FS_TYPE_FAT32;	
	/*
	// read filesystem specific sections
	*/
	switch (fs_type)
	{
		case FAT_FS_TYPE_FAT12:
		case FAT_FS_TYPE_FAT16:
			bpb->BPB_EX.FAT16.BS_DrvNum = *buffer++;
			bpb->BPB_EX.FAT16.BS_Reserved1 = *buffer++;
			bpb->BPB_EX.FAT16.BS_BootSig = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE1] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE2] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE3] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[0] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[1] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[2] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[3] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[4] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[5] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[6] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[7] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[8] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[9] = *buffer++;
			bpb->BPB_EX.FAT16.BS_VolLab[10] = *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[0] = (char) *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[1] = (char) *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[2] = (char) *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[3] = (char) *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[4] = (char) *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[5] = (char) *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[6] = (char) *buffer++;
			bpb->BPB_EX.FAT16.BS_FilSysType[7] = (char) *buffer++;
			break;

		case FAT_FS_TYPE_FAT32:
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE1] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE2] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE3] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_ExtFlags)[INT16_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_ExtFlags)[INT16_BYTE1] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSVer)[INT16_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSVer)[INT16_BYTE1] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE1] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE2] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE3] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSInfo)[INT16_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSInfo)[INT16_BYTE1] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_BkBootSec)[INT16_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BPB_BkBootSec)[INT16_BYTE1] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[0] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[1] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[2] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[3] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[4] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[5] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[6] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[7] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[8] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[9] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[10] = *buffer++;
			bpb->BPB_EX.FAT32.BPB_Reserved[11] = *buffer++;
			bpb->BPB_EX.FAT32.BS_DrvNum = *buffer++;
			bpb->BPB_EX.FAT32.BS_Reserved1 = *buffer++;
			bpb->BPB_EX.FAT32.BS_BootSig = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE0] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE1] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE2] = *buffer++;
			((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE3] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[0] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[1] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[2] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[3] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[4] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[5] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[6] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[7] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[8] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[9] = *buffer++;
			bpb->BPB_EX.FAT32.BS_VolLab[10] = *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[0] = (char) *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[1] = (char) *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[2] = (char) *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[3] = (char) *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[4] = (char) *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[5] = (char) *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[6] = (char) *buffer++;
			bpb->BPB_EX.FAT32.BS_FilSysType[7] = (char) *buffer++;
			break;
	}
}

/*
// writes BPB to buffer
*/
void fat_write_bpb(FAT_BPB* bpb, unsigned char* buffer)
{
	uint32_t no_of_sectors;
	uint32_t no_of_clusters;
	unsigned char fs_type;
	/*
	// write standard bpb
	*/
	*buffer++ = bpb->BS_jmpBoot[0];
	*buffer++ = bpb->BS_jmpBoot[1];
	*buffer++ = bpb->BS_jmpBoot[2];
	*buffer++ = bpb->BS_OEMName[0];
	*buffer++ = bpb->BS_OEMName[1];
	*buffer++ = bpb->BS_OEMName[2];
	*buffer++ = bpb->BS_OEMName[3];
	*buffer++ = bpb->BS_OEMName[4];
	*buffer++ = bpb->BS_OEMName[5];
	*buffer++ = bpb->BS_OEMName[6];
	*buffer++ = bpb->BS_OEMName[7];
	*buffer++ = ((unsigned char*) &bpb->BPB_BytsPerSec)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_BytsPerSec)[INT16_BYTE1];
	*buffer++ = bpb->BPB_SecPerClus;
	*buffer++ = ((unsigned char*) &bpb->BPB_RsvdSecCnt)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_RsvdSecCnt)[INT16_BYTE1];
	*buffer++ = bpb->BPB_NumFATs;
	*buffer++ = ((unsigned char*) &bpb->BPB_RootEntCnt)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_RootEntCnt)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &bpb->BPB_TotSec16)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_TotSec16)[INT16_BYTE1];
	*buffer++ = bpb->BPB_Media;
	*buffer++ = ((unsigned char*) &bpb->BPB_FATSz16)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_FATSz16)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &bpb->BPB_SecPerTrk)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_SecPerTrk)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &bpb->BPB_NumHeads)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_NumHeads)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &bpb->BPB_HiddSec)[INT32_BYTE3];
	*buffer++ = ((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &bpb->BPB_TotSec32)[INT32_BYTE3];
	/*
	// determine filesystem type
	*/
	no_of_sectors = (bpb->BPB_TotSec16) ? bpb->BPB_TotSec16 : bpb->BPB_TotSec32;	
	no_of_clusters = no_of_sectors / bpb->BPB_SecPerClus;
	fs_type = (no_of_clusters < 4085) ? FAT_FS_TYPE_FAT12 :
		(no_of_clusters < 65525) ? FAT_FS_TYPE_FAT16 : FAT_FS_TYPE_FAT32;	
	/*
	// write filesystem specific sections
	*/
	switch (fs_type)
	{
		case FAT_FS_TYPE_FAT12:
		case FAT_FS_TYPE_FAT16:
			*buffer++ = bpb->BPB_EX.FAT16.BS_DrvNum;
			*buffer++ = bpb->BPB_EX.FAT16.BS_Reserved1;
			*buffer++ = bpb->BPB_EX.FAT16.BS_BootSig;
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE1];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE2];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT16.BS_VolID)[INT32_BYTE3];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[0];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[1];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[2];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[3];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[4];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[5];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[6];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[7];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[8];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[9];
			*buffer++ = bpb->BPB_EX.FAT16.BS_VolLab[10];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[0];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[1];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[2];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[3];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[4];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[5];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[6];
			*buffer++ = bpb->BPB_EX.FAT16.BS_FilSysType[7];
			break;

		case FAT_FS_TYPE_FAT32:
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE1];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE2];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FATSz32)[INT32_BYTE3];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_ExtFlags)[INT16_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_ExtFlags)[INT16_BYTE1];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSVer)[INT16_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSVer)[INT16_BYTE1];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE1];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE2];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_RootClus)[INT32_BYTE3];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSInfo)[INT16_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_FSInfo)[INT16_BYTE1];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_BkBootSec)[INT16_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BPB_BkBootSec)[INT16_BYTE1];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[0];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[1];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[2];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[3];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[4];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[5];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[6];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[7];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[8];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[9];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[10];
			*buffer++ = bpb->BPB_EX.FAT32.BPB_Reserved[11];
			*buffer++ = bpb->BPB_EX.FAT32.BS_DrvNum;
			*buffer++ = bpb->BPB_EX.FAT32.BS_Reserved1;
			*buffer++ = bpb->BPB_EX.FAT32.BS_BootSig;
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE0];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE1];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE2];
			*buffer++ = ((unsigned char*) &bpb->BPB_EX.FAT32.BS_VolID)[INT32_BYTE3];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[0];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[1];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[2];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[3];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[4];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[5];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[6];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[7];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[8];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[9];
			*buffer++ = bpb->BPB_EX.FAT32.BS_VolLab[10];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[0];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[1];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[2];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[3];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[4];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[5];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[6];
			*buffer++ = bpb->BPB_EX.FAT32.BS_FilSysType[7];
			break;
	}
}

/*
// reads raw directory entry from buffer
*/
void fat_read_raw_directory_entry(FAT_RAW_DIRECTORY_ENTRY* entry, unsigned char* buffer)
{
	entry->ENTRY.STD.name[0] = *buffer++;
	entry->ENTRY.STD.name[1] = *buffer++;
	entry->ENTRY.STD.name[2] = *buffer++;
	entry->ENTRY.STD.name[3] = *buffer++;
	entry->ENTRY.STD.name[4] = *buffer++;
	entry->ENTRY.STD.name[5] = *buffer++;
	entry->ENTRY.STD.name[6] = *buffer++;
	entry->ENTRY.STD.name[7] = *buffer++;
	entry->ENTRY.STD.name[8] = *buffer++;
	entry->ENTRY.STD.name[9] = *buffer++;
	entry->ENTRY.STD.name[10] = *buffer++;
	entry->ENTRY.STD.attributes = *buffer++;
	entry->ENTRY.STD.reserved = *buffer++;
	entry->ENTRY.STD.create_time_tenth = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.create_time)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.create_time)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.create_date)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.create_date)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.access_date)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.access_date)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.first_cluster_hi)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.first_cluster_hi)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.modify_time)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.modify_time)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.modify_date)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.modify_date)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.first_cluster_lo)[INT16_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.first_cluster_lo)[INT16_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE0] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE1] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE2] = *buffer++;
	((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE3] = *buffer++;
}

/*
// writes raw directory entry to buffer
*/
void fat_write_raw_directory_entry(FAT_RAW_DIRECTORY_ENTRY* entry, unsigned char* buffer)
{
	*buffer++ = entry->ENTRY.STD.name[0];
	*buffer++ = entry->ENTRY.STD.name[1];
	*buffer++ = entry->ENTRY.STD.name[2];
	*buffer++ = entry->ENTRY.STD.name[3];
	*buffer++ = entry->ENTRY.STD.name[4];
	*buffer++ = entry->ENTRY.STD.name[5];
	*buffer++ = entry->ENTRY.STD.name[6];
	*buffer++ = entry->ENTRY.STD.name[7];
	*buffer++ = entry->ENTRY.STD.name[8];
	*buffer++ = entry->ENTRY.STD.name[9];
	*buffer++ = entry->ENTRY.STD.name[10];
	*buffer++ = entry->ENTRY.STD.attributes;
	*buffer++ = entry->ENTRY.STD.reserved;
	*buffer++ = entry->ENTRY.STD.create_time_tenth;
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.create_time)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.create_time)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.create_date)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.create_date)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.access_date)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.access_date)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.first_cluster_hi)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.first_cluster_hi)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.modify_time)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.modify_time)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.modify_date)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.modify_date)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.first_cluster_lo)[INT16_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.first_cluster_lo)[INT16_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE0];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE1];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE2];
	*buffer++ = ((unsigned char*) &entry->ENTRY.STD.size)[INT32_BYTE3];
}
#endif
