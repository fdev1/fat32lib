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

#include <ctype.h>
#include <string.h>
#include <time.h>
#include "fat.h"
#include "fat_internals.h"

#if defined(FAT_FORMAT_UTILITY)

//
// structure of the disk size to sectors per
// cluster lookup table
//
// Taken from:
// Microsoft Extensible Firmware Initiative
// FAT32 File System Specification
//
typedef struct DSKSZTOSECPERCLUS {
	uint32_t DiskSize;
	unsigned char SecPerClusVal;
}
DSKSZTOSECPERCLUS;

/* 
 * This is the table for FAT16 drives. NOTE that this table includes
 * entries for disk sizes larger than 512 MB even though typically
 * only the entries for disks < 512 MB in size are used.
 * The way this table is accessed is to look for the first entry
 * in the table for which the disk size is less than or equal
 * to the DiskSize field in that table entry.  For this table to
 * work properly BPB_RsvdSecCnt must be 1, BPB_NumFATs
 * must be 2, and BPB_RootEntCnt must be 512. Any of these values
 * being different may require the first table entries DiskSize value
 * to be changed otherwise the cluster count may be to low for FAT16.
 */
static const DSKSZTOSECPERCLUS DskTableFAT16 [] = {
   { 8400, 0 }, 		/* disks up to 4.1 MB, the 0 value for SecPerClusVal trips an error */
   { 32680, 2 },  		/* disks up to 16 MB, 1k cluster */
   { 262144, 4 },		/* disks up to 128 MB, 2k cluster */
   { 524288, 8 },		/* disks up to 256 MB, 4k cluster */
   { 1048576, 16 },		/* disks up to 512 MB, 8k cluster */
   /* The entries after this point are not used unless FAT16 is forced */
   { 2097152, 32 },		/* disks up to 1 GB, 16k cluster */
   { 4194304, 64 },		/* disks up to 2 GB, 32k cluster */
   { 0xFFFFFFFF, 0 }	/* any disk greater than 2GB, 0 value for SecPerClusVal trips an error */
};

/* 
 * This is the table for FAT32 drives. NOTE that this table includes
 * entries for disk sizes smaller than 512 MB even though typically
 * only the entries for disks >= 512 MB in size are used.
 * The way this table is accessed is to look for the first entry
 * in the table for which the disk size is less than or equal
 * to the DiskSize field in that table entry. For this table to
 * work properly BPB_RsvdSecCnt must be 32, and BPB_NumFATs
 * must be 2. Any of these values being different may require the first 
 * table entries DiskSize value to be changed otherwise the cluster count 
 * may be to low for FAT32.
 */
static const DSKSZTOSECPERCLUS DskTableFAT32[ ] = {
    { 66600, 0 },		/* disks up to 32.5 MB, the 0 value for SecPerClusVal trips an error */
    { 532480, 1 },		/* disks up to 260 MB, .5k cluster */
    { 16777216, 8 },	/* disks up to 8 GB, 4k cluster */
    { 33554432, 16 },	/* disks up to 16 GB, 8k cluster */
    { 67108864, 32 },	/* disks up to 32 GB, 16k cluster */
    { 0xFFFFFFFF, 64 }	/* disks greater than 32GB, 32k cluster */
};

uint16_t fat_format_volume
(
	unsigned char fs_type,
	char* const volume_label,
	uint32_t no_of_sectors_per_cluster,
	STORAGE_DEVICE* device
) 
{
	uint16_t ret;
	uint32_t i, c;
	uint32_t total_sectors;
	//uint32_t total_clusters;
	uint32_t root_dir_sectors;
	uint32_t fatsz;

	uint32_t backup_boot_sector;
	uint32_t fsinfo_sector;
	uint32_t no_of_root_entries;
	uint32_t no_of_bytes_per_sector;
	uint32_t no_of_reserved_sectors;
	uint32_t root_cluster;
	uint32_t no_of_clusters;
	uint32_t no_of_fat_tables;
	unsigned char media_type;

	FAT_BPB* bpb;
	FAT_FSINFO* fsinfo;
	FAT_RAW_DIRECTORY_ENTRY* entry;
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	//
	// todo: check for illegal characters in volume label
	//
	if (strlen(volume_label) > 11)
		return FAT_INVALID_VOLUME_LABEL;

	if (!no_of_sectors_per_cluster)
	{

		switch (fs_type)
		{
			case FAT_FS_TYPE_FAT12:
				break;

			case FAT_FS_TYPE_FAT16:
				for( i = 0; i < 8; i++ ) 
				{
					if (DskTableFAT16[i].DiskSize >= total_sectors)
					{
						no_of_sectors_per_cluster = DskTableFAT16[i].SecPerClusVal;
						break;
					}
				}
				break;

			case FAT_FS_TYPE_FAT32:
				for (i = 0; i < 6; i++) 
				{
					if (DskTableFAT32[i].DiskSize >= total_sectors)
					{
						no_of_sectors_per_cluster =  DskTableFAT32[i].SecPerClusVal;	
						break;
					}
				}
				break;
		}
	}

	if (!no_of_sectors_per_cluster)
		return FAT_INVALID_FORMAT_PARAMETERS;
	//
	// get the total capacity of the storage
	// device in bytes
	//
	media_type = 0xF8;
	fsinfo_sector = 1;
	no_of_fat_tables = 2;
	backup_boot_sector = 6;
	root_cluster = 2;
	total_sectors = device->GetTotalSectors(device->Media);
	no_of_root_entries = (fs_type == FAT_FS_TYPE_FAT32) ? 0 : 512;
	no_of_bytes_per_sector = device->GetSectorSize(device->Media);
	no_of_reserved_sectors = (fs_type == FAT_FS_TYPE_FAT32) ? 32 : 1;

	//if (no_of_bytes_per_sector != 512)
	//	return FAT_FORMAT_FAT_SIZE_REQUIRED;
	////
	//// calculate the size of the FAT table. This is based on the formula provided by
	//// Microsoft on the FAT specification with some modifications made to support formatting
	//// of FAT12 volumes. It only works for drives with sector size of 512 bytes.
	////
	//root_dir_sectors = ((no_of_root_entries * 32) + (no_of_bytes_per_sector - 1)) / no_of_bytes_per_sector;
	//i = total_sectors - (no_of_reserved_sectors + root_dir_sectors);
	//c = (256 * no_of_sectors_per_cluster) + no_of_fat_tables;
	//c = (fs_type == FAT_FS_TYPE_FAT32) ? c / 2 : c;
	//c = (fs_type == FAT_FS_TYPE_FAT12) ? (c + (c >> 1)) : c;
	//fatsz = (i + (c - 1)) / c;

	//
	// calculate the count of clusters on the volume without accounting
	// for the space that will be used by the FAT tables
	//
	root_dir_sectors = ((no_of_root_entries * 32) + (no_of_bytes_per_sector - 1)) / no_of_bytes_per_sector;
	no_of_clusters = (total_sectors - no_of_reserved_sectors - root_dir_sectors) / no_of_sectors_per_cluster; //rounds down
	//
	// calculate the FAT table size (ignoring the fact that it won't fit
	// since we're allocating all disk space to clusters
	//
	while (1)
	{
		switch (fs_type)
		{
			case FAT_FS_TYPE_FAT12: 
			{
				fatsz =		(((no_of_clusters + 2) + ((no_of_clusters + 2) >> 1)) + 
								no_of_bytes_per_sector - 1) / no_of_bytes_per_sector; 
				break;
			}
			case FAT_FS_TYPE_FAT16: 
			{
				fatsz =		(((no_of_clusters + 2) * 2) + 
								no_of_bytes_per_sector - 1) / no_of_bytes_per_sector; 
				break;
			}
			case FAT_FS_TYPE_FAT32: 
			{
				fatsz =		(((no_of_clusters + 2) * 4) + 
								no_of_bytes_per_sector - 1) / no_of_bytes_per_sector; 
				break;
			}
		}
		//
		// if the FAT table fits then we're done
		//
		if (no_of_reserved_sectors + (fatsz * no_of_fat_tables) + (no_of_clusters * no_of_sectors_per_cluster) <= total_sectors)
			break;
		//
		// decrease the cluster count
		//
		no_of_clusters--;
	}
	//
	// check that the filesystem type/cluster size supplied is correct.
	//
	switch (fs_type)
	{
		case FAT_FS_TYPE_FAT12:
			if (no_of_clusters > 4084)
				return FAT_INVALID_FORMAT_PARAMETERS;
			break;
		case FAT_FS_TYPE_FAT16:
			if (no_of_clusters > 65524 || no_of_clusters < 4085)
				return FAT_INVALID_FORMAT_PARAMETERS;
			break;
		case FAT_FS_TYPE_FAT32:
			if (no_of_clusters < 65525)
				return FAT_INVALID_FORMAT_PARAMETERS;
			break;
	}

	//
	// set common Bios Parameter Block (BPB) fields.
	//
	bpb = (FAT_BPB*) buffer;
	memcpy(bpb->BS_OEMName, "FAT32LIB", 8);
	bpb->BS_jmpBoot[0] = 0xEB;
	bpb->BS_jmpBoot[1] = 0x00; 
	bpb->BS_jmpBoot[2] = 0x90;
	bpb->BPB_NumHeads = 0x1;
	bpb->BPB_HiddSec = 0x0; 
	bpb->BPB_SecPerTrk = 0;
	bpb->BPB_Media = media_type;
	bpb->BPB_NumFATs = no_of_fat_tables;
	bpb->BPB_BytsPerSec = no_of_bytes_per_sector;
	bpb->BPB_RootEntCnt = no_of_root_entries;
	bpb->BPB_RsvdSecCnt = no_of_reserved_sectors;
	bpb->BPB_SecPerClus = no_of_sectors_per_cluster;
	bpb->BPB_TotSec16 = (fs_type == FAT_FS_TYPE_FAT32 || total_sectors > 0xFFFF) ? 0 : total_sectors;
	bpb->BPB_TotSec32 = (fs_type == FAT_FS_TYPE_FAT32 || total_sectors > 0xFFFF) ? total_sectors : 0;
	bpb->BPB_FATSz16 = (fs_type == FAT_FS_TYPE_FAT32) ? 0 : fatsz;

	if (fs_type == FAT_FS_TYPE_FAT32)
	{
		//
		// set FAT32 specific fields
		//
		bpb->FAT32.BS_DrvNum = 0;
		bpb->FAT32.BS_Reserved1 = 0;
		bpb->FAT32.BS_BootSig = 0x29;
		bpb->FAT32.BPB_FATSz32 = fatsz;
		bpb->FAT32.BPB_ExtFlags = 0;
		bpb->FAT32.BPB_FSVer = 0;
		bpb->FAT32.BPB_RootClus = root_cluster;
		bpb->FAT32.BPB_FSInfo = fsinfo_sector;
		bpb->FAT32.BPB_BkBootSec = backup_boot_sector;
		time((time_t*) &bpb->FAT32.BS_VolID);
		memset(bpb->FAT32.BPB_Reserved, 0, 12);
		memcpy(bpb->FAT32.BS_VolLab, "NO NAME    ", 11);
		memcpy(bpb->FAT32.BS_FilSysType, "FAT32   ", 8);
		//
		// set the volume label
		//
		if ((c = strlen(volume_label)))
		{
			for (i = 0; i < 11; i++)
			{
				if (i < c)
				{
					bpb->FAT32.BS_VolLab[i] = toupper(volume_label[i]);
				}
				else
				{
					bpb->FAT32.BS_VolLab[i] = 0x20;
				}
			}
		}
	}
	else
	{
		//
		// set FAT12/FAT16 specific fields
		//
		bpb->FAT16.BS_DrvNum = 0;
		bpb->FAT16.BS_Reserved1 = 0;
		bpb->FAT16.BS_BootSig = 0x29;
		time((time_t*) &bpb->FAT16.BS_VolID);
		memcpy(bpb->FAT16.BS_VolLab, "NO NAME    ", 11);
		memcpy(bpb->FAT16.BS_FilSysType, "FAT     ", 8);
		//
		// set the volume label
		//
		if ((c = strlen(volume_label)))
		{
			for (i = 0; i < 11; i++)
			{
				if (i < c)
				{
					bpb->FAT16.BS_VolLab[i] = toupper(volume_label[i]);
				}
				else
				{
					bpb->FAT16.BS_VolLab[i] = 0x20;
				}
			}
		}
	}
	//
	// set the boot sector signature
	//
	buffer[510] = 0x55;
	buffer[511] = 0xAA;
	bpb = 0;
	//
	// write boot sector to sector 0
	//
	ret = device->WriteSector(device->Media, 0x0, buffer);
	if (ret != STORAGE_SUCCESS)
		return FAT_CANNOT_WRITE_MEDIA;
	//
	// if this is a FAT32 volume write a backup of boot sector
	// and FSInfo structure
	//
	if (fs_type == FAT_FS_TYPE_FAT32)
	{
		//
		// write a copy of the boot sector to sector # BPB_BkBootSec
		//
		ret = device->WriteSector(device->Media, backup_boot_sector, buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_WRITE_MEDIA;
		//
		// initialize the FSInfo structure
		//
		fsinfo = (FAT_FSINFO*) buffer;
		fsinfo->LeadSig = 0x41615252;
		fsinfo->StructSig = 0x61417272;
		fsinfo->Free_Count = 0xFFFFFFFF;
		fsinfo->Nxt_Free = 0xFFFFFFFF;
		memset(fsinfo->Reserved1, 0, 480);
		memset(fsinfo->Reserved2, 0, 12);
		fsinfo = 0;
		//
		// write the FSInfo structor to sector # BPB_FSInfo
		//
		ret = device->WriteSector(device->Media, fsinfo_sector, buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_READ_MEDIA;
	}
	//
	// zero out the FAT table and set the entries for clusters 0 and 1
	// start by zeroing the whole buffer
	//
	memset(buffer, 0, MAX_SECTOR_LENGTH);
	//
	// loop through each sector on the fat
	//
	for (i = 0; i < fatsz; i++)
	{
		if (!i)
		{
			//
			// if this is the 1st sector of the FAT set the values for
			// clusters 0 and 1
			//
			if (fs_type == FAT_FS_TYPE_FAT12)
			{
				uint16_t value;

				// even
				
				value = 0xFF8;										// the value to be written
				*((uint16_t*) &buffer[0]) &= 0xF000;				// clear bits for entry
				*((uint16_t*) &buffer[0]) |= (value & 0x0FFF);		// set bits for entry
				
				// odd - we need to write this one 1 byte at a time since
				// unaligned word access is inefficient and may cause a fatal
				// exception on some platforms.

				value = FAT12_EOC;							// the value to be written
				value <<= 4;								// an odd entry occupies the upper 12-bits of the word

				buffer[1 + 0] &= 0x0F;						// clear entry bits on 1st byte
				buffer[1 + 0] |= LO8(value);				// set entry bits on 1st byte

				buffer[1 + 1]  = 0x00;						// clear the 2nd byte
				buffer[1 + 1]  = HI8(value);				// set the 2nd byte

				//*((uint16_t*) &buffer[1]) &= 0x000F;
				//*((uint16_t*) &buffer[1]) |= (value << 4);

				// todo: we need to do this 1 byte at the time or we'll
				// run into alignment issues with some processors
				//
				// store the media type in the lower 8 bits of the first entry
				// leave the other 4 bits set to 1
				//
				//((uint16_t*) buffer)[0] = (0x0FF8 & 0xFF00) | media_type;
				//
				// set the 2nd entry to 0xFF6. The 1st 4 bytes goes on the upper
				// 4 bits of the 1st 16 bits of the FAT table and the remaining 8
				// bits go on the lower byte of the 2nd 16-bit entry
				//
				//((uint16_t*) buffer)[0] = (((uint16_t*) buffer)[0] & 0x0FFF) | (0x0FF8 << 12);
				//((uint16_t*) buffer)[1] = (((uint16_t*) buffer)[1] & 0xFF00) | (0x0FF8 >> 4);

			}
			else if (fs_type == FAT_FS_TYPE_FAT16)
			{
				((uint16_t*) buffer)[0] = (FAT16_EOC & 0xFF00) | media_type;
				((uint16_t*) buffer)[1] = FAT16_EOC;
			}
			else
			{
				((uint32_t*) buffer)[0] = (FAT32_EOC & 0x0FFFFF00) | media_type;
				((uint32_t*) buffer)[1] = FAT32_EOC;
				((uint32_t*) buffer)[2] = FAT32_EOC;
			}
		}
		else
		{
			// 
			// if this is not the 1st sector we need to clear the
			// values of the first 1 clusters (because we just set them
			// for the 1st entry). Since the whole sector is set to zeroes
			// it doesn't matter that we're clearing the 1st 64 bits regardless
			// of filesystem type
			//
			((uint32_t*) buffer)[0] = 0x0;
			((uint32_t*) buffer)[1] = 0x0;
			((uint32_t*) buffer)[2] = 0x0;
		}
		//
		// write the sector to all the FAT tables
		//
		for (c = 0; c < no_of_fat_tables; c++)
		{
			ret = device->WriteSector(device->Media, no_of_reserved_sectors + i + (c * fatsz), buffer);
			if (ret != STORAGE_SUCCESS)
				return FAT_CANNOT_WRITE_MEDIA;
		}
	}
	//
	// zero the root directory (or it's 1st cluster in the case of FAT32
	// note: since cluster #2 is located where the root directory would be on
	// a FAT12/FAT16 volume we can use the same code for both, only far
	// FAT32 we only initialize 1 cluster
	//
	// this is the formula to get the 1st sector of a cluster:
	//
	// ((root_cluster - 2) * no_of_sectors_per_cluster) + no_of_reserved_sectors + (no_of_fat_tables * fatsz) + root_dir_sectors;
	//
	// since we're using cluster 2 as the root cluster and root_dir_sectors == 0 for FAT32 this is equal to:
	//
	// no_of_reserved_sectors + (no_of_fat_tables * fatsz)
	//
	// so the address for the root directory is the same on FAT32 and on FAT12/16 as long as we're
	// using cluster #2
	//
	memset(buffer, 0, MAX_SECTOR_LENGTH);
	c = (fs_type == FAT_FS_TYPE_FAT32) ? no_of_sectors_per_cluster : root_dir_sectors;
	for (i = 1; i < c; i++)
	{
		ret = device->WriteSector(device->Media, no_of_reserved_sectors + (no_of_fat_tables * fatsz) + i, buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_WRITE_MEDIA;
	}
	//
	// initialize the volume label entry
	//
	entry = (FAT_RAW_DIRECTORY_ENTRY*) buffer;
	entry->attributes = FAT_ATTR_VOLUME_ID;
	entry->first_cluster_hi = 0;
	entry->first_cluster_lo = 0;
	entry->reserved = 0;
	entry->size = 0;
	entry->create_date = rtc_get_fat_date();
	entry->create_time = rtc_get_fat_time();
	entry->modify_date = entry->create_date;
	entry->modify_time = entry->create_time;
	entry->access_date = entry->create_date;
	entry->create_time_tenth = 0;
	//
	// set the volume label
	//
	if ((c = strlen(volume_label)))
	{
		for (i = 0; i < 11; i++)
		{
			if (i < c)
			{
				entry->name[i] = (volume_label[i]);
			}
			else
			{
				entry->name[i] = 0x20;
			}
		}
	}
	//
	// write the volume label entry to the root dir
	//
	ret = device->WriteSector(device->Media, no_of_reserved_sectors + (no_of_fat_tables * fatsz), buffer);
	if (ret != STORAGE_SUCCESS)
		return FAT_CANNOT_WRITE_MEDIA;
	//
	// return success
	//	
	return FAT_SUCCESS;
}	

#endif //FAT_FORMAT_UTILITY

void fat_parse_path(char* path, char* path_part, char** filename_part)
{
	*filename_part = path + strlen(path);
	while (*--(*filename_part) != '\\' && (*filename_part) != path);
	
	while (path != *filename_part)
		*path_part++ = *path++;
	*path_part = 0;
	(*filename_part)++;
}
