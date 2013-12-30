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

#include <ctype.h>
#include <string.h>
#include <time.h>
#include "fat.h"
#include "fat_internals.h"

#if defined(FAT_FORMAT_UTILITY)

/*
// structure of the disk size to sectors per
// cluster lookup table
*/
typedef struct DSKSZTOSECPERCLUS 
{
	uint32_t DiskSize;
	unsigned char SecPerClusVal;
}
DSKSZTOSECPERCLUS;

/*
// prototypes
*/
static unsigned char fat_get_cluster_size(unsigned char fs_type, uint32_t total_sectors, char forced_fs_type);

/*
// this is the table for FAT12 drives.
*/
#if !defined(FAT_READ_ONLY)
static const DSKSZTOSECPERCLUS DskTableFAT12[] =
{
	{ 8200, 2 },  		/* disks up to 4 MB, 1k cluster */
	{ 16368, 4 },		/* disks up to 8 MB, 2k cluster */
	{ 32704, 8 },		/* disks up to 16 MB, 4k cluster */
	/* The entries after this point are not used unless FAT16 is forced */
	{ 65376, 16 },		/* disks up to 32 MB, 8k cluster */
	{ 130720, 32 },		/* disks up to 64 MB, 16k cluster */
	{ 261408, 64 },		/* disks up to 128 MB, 32k cluster */
	{ 0xFFFFFFFF, 0 }	/* any disk greater than 2GB, 0 value for SecPerClusVal trips an error */
};
#endif

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
#if !defined(FAT_READ_ONLY)
static const DSKSZTOSECPERCLUS DskTableFAT16[] = 
{
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
#endif

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
#if !defined(FAT_READ_ONLY)
static const DSKSZTOSECPERCLUS DskTableFAT32[ ] = 
{
    { 66600, 0 },		/* disks up to 32.5 MB, the 0 value for SecPerClusVal trips an error */
    { 532480, 1 },		/* disks up to 260 MB, .5k cluster */
    { 16777216, 8 },	/* disks up to 8 GB, 4k cluster */
    { 33554432, 16 },	/* disks up to 16 GB, 8k cluster */
    { 67108864, 32 },	/* disks up to 32 GB, 16k cluster */
    { 0xFFFFFFFF, 64 }	/* disks greater than 32GB, 32k cluster */
};
#endif

/*
// find the best cluster size of given file system and # of 512 bytes sectors
*/
#if !defined(FAT_READ_ONLY)
static unsigned char fat_get_cluster_size(unsigned char fs_type, uint32_t total_sectors, char forced_fs_type)
{
	unsigned char i;
	switch (fs_type)
	{
		case FAT_FS_TYPE_FAT12:
			for( i = 0; i < forced_fs_type ? 7 : 3; i++ ) 
			{
				if (DskTableFAT12[i].DiskSize >= total_sectors)
				{
					return DskTableFAT12[i].SecPerClusVal;
				}
			}
			break;

		case FAT_FS_TYPE_FAT16:
			for( i = 0; i < forced_fs_type ? 8 : 5; i++ ) 
			{
				if (DskTableFAT16[i].DiskSize >= total_sectors)
				{
					return DskTableFAT16[i].SecPerClusVal;
				}
			}
			break;

		case FAT_FS_TYPE_FAT32:
			for (i = 0; i < 6; i++) 
			{
				if (DskTableFAT32[i].DiskSize >= total_sectors)
				{
					return DskTableFAT32[i].SecPerClusVal;	
				}
			}
			break;
	}
	return 0;
}
#endif

/*
// format volume
*/
uint16_t fat_format_volume
(
	unsigned char fs_type,
	char* const volume_label,
	uint32_t no_of_sectors_per_cluster,
	STORAGE_DEVICE* device
) 
{
	#if defined(FAT_READ_ONLY)
	return FAT_FEATURE_NOT_SUPPORTED;
	#else
	uint16_t ret;
	uint32_t i, c;
	uint32_t total_sectors;
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
	uint32_t root_entry_sector;
	uint32_t root_entry_offset;

	FAT_BPB* bpb;
	FAT_FSINFO* fsinfo;
	FAT_RAW_DIRECTORY_ENTRY* entry;
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	FAT_BPB bpb_mem;
	FAT_RAW_DIRECTORY_ENTRY entry_mem;
	#endif
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	/*
	// todo: check for illegal characters in volume label
	*/
	if (strlen(volume_label) > 11)
		return FAT_INVALID_VOLUME_LABEL;
	/*
	// get the total capacity of the storage
	// device in bytes
	*/
	media_type = 0xF8;
	fsinfo_sector = 1;
	no_of_fat_tables = 2;
	backup_boot_sector = 6;
	root_cluster = 2;
	total_sectors = device->get_total_sectors(device->driver);
	no_of_bytes_per_sector = device->get_sector_size(device->driver);
	/*
	// if the user didn't specify a file system type find the
	// most appropriate one
	*/
	if (fs_type == FAT_FS_TYPE_UNSPECIFIED)
	{
		/*
		// if the user specifies a cluster size he/she must also
		// specify a file system type. Also we can only calculate cluster
		// size for drives with a 512 bytes sector
		*/
		if (no_of_sectors_per_cluster || no_of_bytes_per_sector != 0x200)
			return FAT_INVALID_FORMAT_PARAMETERS;
		/*
		// first try FAT12
		*/
		no_of_sectors_per_cluster = fat_get_cluster_size(FAT_FS_TYPE_FAT12, total_sectors, 0);

		if (no_of_sectors_per_cluster)
		{
			fs_type = FAT_FS_TYPE_FAT12;
		}
		else
		{
			/*
			// try FAT16
			*/
			no_of_sectors_per_cluster = fat_get_cluster_size(FAT_FS_TYPE_FAT16, total_sectors, 0);

			if (no_of_sectors_per_cluster)
			{
				fs_type = FAT_FS_TYPE_FAT16;
			}
			else
			{
				/*
				// try FAT32
				*/
				no_of_sectors_per_cluster = fat_get_cluster_size(FAT_FS_TYPE_FAT32, total_sectors, 0);

				if (no_of_sectors_per_cluster)
				{
					fs_type = FAT_FS_TYPE_FAT32;
				}
			}
		}
	}
	/*
	// if we don't have a valid fs type return error
	*/
	if (fs_type < FAT_FS_TYPE_FAT12 || fs_type > FAT_FS_TYPE_FAT32)
		return FAT_INVALID_FORMAT_PARAMETERS;
	/*
	// if we don't have a cluster size try to get one
	*/
	if (!no_of_sectors_per_cluster)
	{
		/*
		// if sector size is not 512 bytes user must specify cluster size
		*/
		if (no_of_sectors_per_cluster || no_of_bytes_per_sector != 0x200)
			return FAT_INVALID_FORMAT_PARAMETERS;
		no_of_sectors_per_cluster = fat_get_cluster_size(fs_type, total_sectors, 1);
	}
	/*
	// if we still don't have it return error
	*/
	if (!no_of_sectors_per_cluster)
		return FAT_INVALID_FORMAT_PARAMETERS;
	/*
	// calculate no of root entries and reserved sectors
	*/
	no_of_root_entries = (fs_type == FAT_FS_TYPE_FAT32) ? 0 : 512;
	no_of_reserved_sectors = (fs_type == FAT_FS_TYPE_FAT32) ? 32 : 1;
	/*
	// calculate the count of clusters on the volume without accounting
	// for the space that will be used by the FAT tables
	*/
	root_dir_sectors = ((no_of_root_entries * 32) + (no_of_bytes_per_sector - 1)) / no_of_bytes_per_sector;
	no_of_clusters = (total_sectors - no_of_reserved_sectors - root_dir_sectors) / no_of_sectors_per_cluster; /*rounds down*/
	/*
	// calculate the FAT table size (ignoring the fact that it won't fit
	// since we're allocating all disk space to clusters
	*/
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
		/*
		// if the FAT table fits then we're done
		*/
		if (no_of_reserved_sectors + (fatsz * no_of_fat_tables) + (no_of_clusters * no_of_sectors_per_cluster) <= total_sectors)
			break;
		/*
		// decrease the cluster count
		*/
		no_of_clusters--;
	}
	/*
	// check that the filesystem type/cluster size supplied is correct.
	*/
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
	root_cluster = 3; /* no_of_clusters - 100; */
	/*
	// calculate the offset of the cluster's FAT entry within it's sector
	// note: when we hit get past the end of the current sector entry_offset
	// will roll back to zero (or possibly 1 for FAT12)
	*/
	switch (fs_type)
	{
		case FAT_FS_TYPE_FAT12: root_entry_offset = root_cluster + (root_cluster >> 1); break;
		case FAT_FS_TYPE_FAT16: root_entry_offset = root_cluster * ((uint32_t) 2); break;
		case FAT_FS_TYPE_FAT32: root_entry_offset = root_cluster * ((uint32_t) 4); break;
	}
	root_entry_sector = (root_entry_offset / no_of_bytes_per_sector);
	root_entry_offset = (root_entry_offset % no_of_bytes_per_sector) / 4; 
	/*
	// set common Bios Parameter Block (BPB) fields.
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	bpb = &bpb_mem;
	#else
	bpb = (FAT_BPB*) buffer;
	#endif
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
		/*
		// set FAT32 specific fields
		*/
		bpb->BPB_EX.FAT32.BS_DrvNum = 0;
		bpb->BPB_EX.FAT32.BS_Reserved1 = 0;
		bpb->BPB_EX.FAT32.BS_BootSig = 0x29;
		bpb->BPB_EX.FAT32.BPB_FATSz32 = fatsz;
		bpb->BPB_EX.FAT32.BPB_ExtFlags = 0;
		bpb->BPB_EX.FAT32.BPB_FSVer = 0;
		bpb->BPB_EX.FAT32.BPB_RootClus = root_cluster;
		bpb->BPB_EX.FAT32.BPB_FSInfo = fsinfo_sector;
		bpb->BPB_EX.FAT32.BPB_BkBootSec = backup_boot_sector;
		time((time_t*) &bpb->BPB_EX.FAT32.BS_VolID);
		memset(bpb->BPB_EX.FAT32.BPB_Reserved, 0, 12);
		memcpy(bpb->BPB_EX.FAT32.BS_VolLab, "NO NAME    ", 11);
		memcpy(bpb->BPB_EX.FAT32.BS_FilSysType, "FAT32   ", 8);
		/*
		// set the volume label
		*/
		if ((c = strlen(volume_label)))
		{
			for (i = 0; i < 11; i++)
			{
				if (i < c)
				{
					bpb->BPB_EX.FAT32.BS_VolLab[i] = toupper(volume_label[i]);
				}
				else
				{
					bpb->BPB_EX.FAT32.BS_VolLab[i] = 0x20;
				}
			}
		}
	}
	else
	{
		/*
		// set FAT12/FAT16 specific fields
		*/
		bpb->BPB_EX.FAT16.BS_DrvNum = 0;
		bpb->BPB_EX.FAT16.BS_Reserved1 = 0;
		bpb->BPB_EX.FAT16.BS_BootSig = 0x29;
		time((time_t*) &bpb->BPB_EX.FAT16.BS_VolID);
		memcpy(bpb->BPB_EX.FAT16.BS_VolLab, "NO NAME    ", 11);
		memcpy(bpb->BPB_EX.FAT16.BS_FilSysType, "FAT     ", 8);
		/*
		// set the volume label
		*/
		if ((c = strlen(volume_label)))
		{
			for (i = 0; i < 11; i++)
			{
				if (i < c)
				{
					bpb->BPB_EX.FAT16.BS_VolLab[i] = toupper(volume_label[i]);
				}
				else
				{
					bpb->BPB_EX.FAT16.BS_VolLab[i] = 0x20;
				}
			}
		}
	}
	/*
	// write bpb to buffer 
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	fat_write_bpb(bpb, buffer);
	#endif
	/*
	// set the boot sector signature
	*/
	buffer[510] = 0x55;
	buffer[511] = 0xAA;
	bpb = 0;
	/*
	// write boot sector to sector 0
	*/
	ret = device->write_sector(device->driver, 0x0, buffer);
	if (ret != STORAGE_SUCCESS)
		return FAT_CANNOT_WRITE_MEDIA;
	/*
	// if this is a FAT32 volume write a backup of boot sector
	// and FSInfo structure
	*/
	if (fs_type == FAT_FS_TYPE_FAT32)
	{
		/*
		// write a copy of the boot sector to sector # BPB_BkBootSec
		*/
		ret = device->write_sector(device->driver, backup_boot_sector, buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_WRITE_MEDIA;
		/*
		// initialize the FSInfo structure
		*/
		fsinfo = (FAT_FSINFO*) buffer;
		fsinfo->LeadSig = 0x41615252;
		fsinfo->StructSig = 0x61417272;
		fsinfo->Free_Count = no_of_clusters - 1;
		fsinfo->Nxt_Free = 3;
		fsinfo->TrailSig = 0xAA550000;
		memset(fsinfo->Reserved1, 0, 480);
		memset(fsinfo->Reserved2, 0, 12);
		fsinfo = 0;
		/*
		// write the FSInfo structor to sector # BPB_FSInfo
		*/
		ret = device->write_sector(device->driver, fsinfo_sector, buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_READ_MEDIA;
	}



	for (c = 0; c < no_of_fat_tables; c++)
	{
		/*
		// zero out the FAT table and set the entries for clusters 0 and 1
		// start by zeroing the whole buffer
		*/
		memset(buffer, 0, MAX_SECTOR_LENGTH);
		/*
		// loop through each sector on the fat
		*/
		for (i = 0; i < fatsz; i++)
		{
			if (!i)
			{
				/*
				// if this is the 1st sector of the FAT set the values for
				// clusters 0 and 1
				*/
				if (fs_type == FAT_FS_TYPE_FAT12)
				{
					uint16_t value;

					/* even */
					
					value = 0xFF8;										/* the value to be written */
					*((uint16_t*) &buffer[0]) &= 0xF000;				/* clear bits for entry */
					*((uint16_t*) &buffer[0]) |= (value & 0x0FFF);		/* set bits for entry */
					/*
					// odd - we need to write this one 1 byte at a time since
					// unaligned word access is inefficient and may cause a fatal
					// exception on some platforms.
					*/
					value = FAT12_EOC;							/* the value to be written */
					value <<= 4;								/* an odd entry occupies the upper 12-bits of the word */

					buffer[1 + 0] &= 0x0F;						/* clear entry bits on 1st byte */
					buffer[1 + 0] |= LO8(value);				/* set entry bits on 1st byte */

					buffer[1 + 1]  = 0x00;						/* clear the 2nd byte */
					buffer[1 + 1]  = HI8(value);				/* set the 2nd byte */

				}
				else if (fs_type == FAT_FS_TYPE_FAT16)
				{
					#if defined(BIG_ENDIAN)
					buffer[(0 * 2) + 0] = LO8((FAT16_EOC & 0xFF00) | media_type);
					buffer[(0 * 2) + 1] = HI8((FAT16_EOC & 0xFF00) | media_type);
					buffer[(1 * 2) + 0] = LO8(FAT16_EOC);
					buffer[(1 * 2) + 1] = HI8(FAT16_EOC);
					#else
					((uint16_t*) buffer)[0] = (FAT16_EOC & 0xFF00) | media_type;
					((uint16_t*) buffer)[1] = FAT16_EOC;
					#endif
				}
				else
				{
					#if defined(BIG_ENDIAN)
					buffer[(0 * 4) + 0] = LO8(LO16((FAT32_EOC & 0x0FFFFF00) | media_type));
					buffer[(0 * 4) + 1] = HI8(LO16((FAT32_EOC & 0x0FFFFF00) | media_type));
					buffer[(0 * 4) + 2] = LO8(HI16((FAT32_EOC & 0x0FFFFF00) | media_type));
					buffer[(0 * 4) + 3] = HI8(HI16((FAT32_EOC & 0x0FFFFF00) | media_type));

					buffer[(1 * 4) + 0] = LO8(LO16(FAT32_EOC));
					buffer[(1 * 4) + 1] = HI8(LO16(FAT32_EOC));
					buffer[(1 * 4) + 2] = LO8(HI16(FAT32_EOC));
					buffer[(1 * 4) + 3] = HI8(HI16(FAT32_EOC));

					buffer[(2 * 4) + 0] = LO8(LO16(FAT32_EOC));
					buffer[(2 * 4) + 1] = HI8(LO16(FAT32_EOC));
					buffer[(2 * 4) + 2] = LO8(HI16(FAT32_EOC));
					buffer[(2 * 4) + 3] = HI8(HI16(FAT32_EOC));

					#else
					((uint32_t*) buffer)[0] = (FAT32_EOC & 0x0FFFFF00) | media_type;
					((uint32_t*) buffer)[1] = FAT32_EOC;
					/*((uint32_t*) buffer)[2] = FAT32_EOC;*/
					#endif
				}
			}
			if (i == root_entry_sector && fs_type == FAT_FS_TYPE_FAT32)
			{
				((uint32_t*) buffer)[root_entry_offset] = FAT32_EOC;
			}

			/*
			// write the sector to all the FAT tables
			*/
			ret = device->write_sector(device->driver, no_of_reserved_sectors + i + (c * fatsz), buffer);
			if (ret != STORAGE_SUCCESS)
				return FAT_CANNOT_WRITE_MEDIA;

			if (i == 0 || i == root_entry_sector)
			{
				memset(buffer, 0, MAX_SECTOR_LENGTH);
			}
		}
	}
	/*
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
	*/
	memset(buffer, 0, MAX_SECTOR_LENGTH);
	c = (fs_type == FAT_FS_TYPE_FAT32) ? no_of_sectors_per_cluster : root_dir_sectors;
	for (i = 1; i < c; i++)
	{
		if (fs_type == FAT_FS_TYPE_FAT32)
		{
			uint32_t root_sector = ((root_cluster - 2) * no_of_sectors_per_cluster) + no_of_reserved_sectors + (no_of_fat_tables * fatsz) + root_dir_sectors;
			ret = device->write_sector(device->driver, root_sector, buffer);
			if (ret != STORAGE_SUCCESS)
				return FAT_CANNOT_WRITE_MEDIA;
		}
		else
		{
			ret = device->write_sector(device->driver, no_of_reserved_sectors + (no_of_fat_tables * fatsz) + i, buffer);
			if (ret != STORAGE_SUCCESS)
				return FAT_CANNOT_WRITE_MEDIA;

		}
	}
	/*
	// initialize the volume label entry
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	entry = &entry_mem;
	#else
	entry = (FAT_RAW_DIRECTORY_ENTRY*) buffer;
	#endif
	entry->ENTRY.STD.attributes = FAT_ATTR_VOLUME_ID;
	entry->ENTRY.STD.first_cluster_hi = 0;
	entry->ENTRY.STD.first_cluster_lo = 0;
	entry->ENTRY.STD.reserved = 0;
	entry->ENTRY.STD.size = 0;
	entry->ENTRY.STD.create_date = rtc_get_fat_date();
	entry->ENTRY.STD.create_time = rtc_get_fat_time();
	entry->ENTRY.STD.modify_date = entry->ENTRY.STD.create_date;
	entry->ENTRY.STD.modify_time = entry->ENTRY.STD.create_time;
	entry->ENTRY.STD.access_date = entry->ENTRY.STD.create_date;
	entry->ENTRY.STD.create_time_tenth = 0;
	/*
	// set the volume label
	*/
	if ((c = strlen(volume_label)))
	{
		for (i = 0; i < 11; i++)
		{
			if (i < c)
			{
				entry->ENTRY.STD.name[i] = (volume_label[i]);
			}
			else
			{
				entry->ENTRY.STD.name[i] = 0x20;
			}
		}
	}
	/*
	// if necessary, write the  entry to the buffer
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	fat_write_raw_directory_entry(entry, buffer);
	#endif
	/*
	// write the volume label entry to the root dir
	*/
	if (fs_type == FAT_FS_TYPE_FAT32)
	{
		ret = device->write_sector(device->driver, ((root_cluster - 2) * no_of_sectors_per_cluster) + no_of_reserved_sectors + (no_of_fat_tables * fatsz) + root_dir_sectors, buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_WRITE_MEDIA;
	}
	else
	{
		ret = device->write_sector(device->driver, no_of_reserved_sectors + (no_of_fat_tables * fatsz), buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_WRITE_MEDIA;
	}
	/*
	// return success
	*/	
	return FAT_SUCCESS;
	#endif
}	

#endif /* FAT_FORMAT_UTILITY */
