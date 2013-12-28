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

#ifndef FAT_INTERNALS
#define FAT_INTERNALS

#include "fat.h"

#define FAT12_EOC						( 0x0FFF )
#define FAT16_EOC						( 0xFFFF )
#define FAT32_EOC						( 0x0FFFFFFF )
#define FAT12_BAD_CLUSTER				( 0x0FF7 )
#define FAT16_BAD_CLUSTER				( 0xFFF7 )
#define FAT32_BAD_CLUSTER				( 0x0FFFFFF7 )
#define FAT16_CLEAN_SHUTDOWN			( 0x8000 )
#define FAT32_CLEAN_SHUTDOWN			( 0x08000000 )
#define FAT16_HARD_ERROR				( 0x4000 )
#define FAT32_HARD_ERROR				( 0x04000000 )
#define FAT32_CUTOVER					( 1024 )
#define FREE_FAT						( 0x0000 )
#define ILLEGAL_CHARS_COUNT 			( 0x10 )
#define BACKSLASH						( 0x5C )
#define FAT_OPEN_HANDLE_MAGIC			( 0x4B )
#define FAT_DELETED_ENTRY				( 0xE5 )
#define FAT_UNKNOWN_SECTOR				( 0xFFFFFFFF )

/*
// macro for computing the 1st sector of a cluster
*/
#define FIRST_SECTOR_OF_CLUSTER(volume, cluster) 	\
	(((cluster - 0x2) * volume->no_of_sectors_per_cluster) + \
	volume->first_data_sector)

/*
// macro for checking if an entry in the FAT is free
*/	
#define IS_FREE_FAT(volume, fat)	\
	((volume->fs_type == FAT_FS_TYPE_FAT32) ? !(fat & 0x0FFFFFFF) : \
	(volume->fs_type == FAT_FS_TYPE_FAT16) ? !(fat & 0xFFFF) : !(fat & 0x0FFF))

/*
// macros for checking if a directory entry is free
// and if it's the last entry on the directory
*/
#define IS_FREE_DIRECTORY_ENTRY(entry) (*(entry)->ENTRY.STD.name == 0xE5 || *(entry)->ENTRY.STD.name == 0x0)
#define IS_LAST_DIRECTORY_ENTRY(entry) (*(entry)->ENTRY.STD.name == 0x0)

/*
// date/time macros
*/
#define FAT_ENCODE_DATE(month, day, year)			((((uint16_t)((year) - 1980)) << 9) | ((uint16_t)(month) << 5) | (uint16_t)(day))
#define FAT_DECODE_DATE(date, month, day, year)		(year) = ((date) >> 9) + 1980);(month) = ((date & 0x1E0) >> 5);(day) = (date & 0x1F)
#define FAT_ENCODE_TIME(hour, minute, second)		(((uint16_t)(hour) << 11) | ((uint16_t)(minute) << 5) | ((uint16_t)(second) >> 1))
#define FAT_DECODE_TIME(time, hour, minute, second)	(hour) = ((time) >> 11); (minute) = (((time) & 0x7E0) >> 5); (secs) = (((time) & 0x1F) << 1)

/*
// min and max macros
*/	
#define MAX( a, b )					( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MIN( a, b )					( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define LO8( word )					( ( unsigned char ) (word) )
#define HI8( word )					( ( unsigned char ) ( (word) >> 8 ) )
#define LO16( dword )				( ( uint16_t ) (dword) )
#define HI16( dword )				( ( uint16_t ) ( (dword) >> 16 ) )
/* #define INDEXOF(chr, str, idx)		((int16_t) ((int16_t) strchr(str + idx, chr) - (int16_t)str)) */


/*
// FAT entry data type
*/
typedef uint32_t FAT_ENTRY;

/*
//
// fat 32-byte directory entry structure
//
typedef struct _DIRECTORY_ENTRY {
	PACKED char Name[ 11 ];
	PACKED unsigned char Attr;
	PACKED unsigned char NTRes;
	PACKED unsigned char CrtTimeTenth;
	PACKED uint16_t CrtTime;
	PACKED uint16_t CrtDate;
	PACKED uint16_t LastAccDate;
	PACKED uint16_t FstClusHI;
	PACKED uint16_t WrtTime;
	PACKED uint16_t WrtDate;
	PACKED uint16_t FstClusLO;
	PACKED uint32_t FileSize;
}
FAT_RAW_DIRECTORY_ENTRY;
*/
/*
// stores the state of a FAT query
//
//typedef struct _FAT_QUERY_STATE {
//	unsigned char Attributes;
//	uint16_t CurrentSector;
//	uint32_t CurrentCluster;
//	FAT_RAW_DIRECTORY_ENTRY* FirstEntry;
//	FAT_RAW_DIRECTORY_ENTRY* CurrentEntry;
//	unsigned char CurrentSectorData[ MAX_SECTOR_LENGTH ];
//}	
//FAT_QUERY_STATE;
*/

/*
// file system query structure
*/
typedef struct _FILESYSTEM_QUERY_INNER {
	FAT_DIRECTORY_ENTRY current_entry;
	FAT_QUERY_STATE state;
}	
FAT_FILESYSTEM_QUERY_INNER;

/*
// FAT32 FSInfo structure
*/
#if defined(USE_PRAGMA_PACK)
#pragma pack(1)
#endif
BEGIN_PACKED_STRUCT
typedef struct FAT_FSINFO {
	PACKED uint32_t TrailSig;
	PACKED unsigned char Reserved2[ 12 ];
	PACKED uint32_t Nxt_Free;
	PACKED uint32_t Free_Count;
	PACKED uint32_t StructSig;
	PACKED unsigned char Reserved1[ 480 ];
	PACKED uint32_t LeadSig;
}
FAT_FSINFO;
END_PACKED_STRUCT
#if defined(USE_PRAGMA_PACK)
#pragma pack()
#endif

/*
// table of illegal filename chars.
*/
static const char ILLEGAL_CHARS[] = { 
	0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B, 
	0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C 
};

/*
// MBR partition entry structure
*/
#if defined(USE_PRAGMA_PACK)
#pragma pack(1)
#endif
BEGIN_PACKED_STRUCT
typedef struct FAT_PARTITION_ENTRY
{
	PACKED unsigned char status;
	PACKED unsigned char chs_first_sector[3];
	PACKED unsigned char partition_type;
	PACKED unsigned char chs_last_sector[3];
	PACKED uint32_t lba_first_sector;
	PACKED uint32_t total_sectors;
}
FAT_PARTITION_ENTRY;
#if defined(USE_PRAGMA_PACK)
#pragma pack()
#endif

/*
// BPB structure ( 224 bits/28 bytes )
*/
#if defined(USE_PRAGMA_PACK)
#pragma pack(1)
#endif
BEGIN_PACKED_STRUCT
typedef struct FAT_BPB
{
	PACKED unsigned char BS_jmpBoot[3];				/* 0  */
	PACKED char BS_OEMName[8];						/* 3  */
	PACKED uint16_t BPB_BytsPerSec;					/* 11 */
	PACKED unsigned char BPB_SecPerClus;			/* 13 */
	PACKED uint16_t BPB_RsvdSecCnt;					/* 14 */
	PACKED unsigned char BPB_NumFATs;				/* 16 */
	PACKED uint16_t BPB_RootEntCnt;					/* 17 */
	PACKED uint16_t BPB_TotSec16;					/* 19 */
	PACKED unsigned char BPB_Media;					/* 21 */
	PACKED uint16_t BPB_FATSz16;					/* 22 */
	PACKED uint16_t BPB_SecPerTrk;					/* 24 */
	PACKED uint16_t BPB_NumHeads;					/* 26 */
	PACKED uint32_t BPB_HiddSec;					/* 28 */
	PACKED uint32_t BPB_TotSec32;					/* 32 */
	union 
	{
		/*
		// 62
		*/
		struct FAT16
		{ /* fat16 */
			PACKED unsigned char BS_DrvNum;			/* 36 */
			PACKED unsigned char BS_Reserved1;		/* 37 */
			PACKED unsigned char BS_BootSig;		/* 38 */
			PACKED uint32_t BS_VolID;				/* 39 */
			PACKED char BS_VolLab[ 11 ];			/* 43 */
			PACKED char BS_FilSysType[8];			/* 54 */
			PACKED char Pad1[8];
			/* PACKED uint64_t Pad1; */
			PACKED uint32_t Pad2;
			PACKED unsigned char Pad3;
			PACKED unsigned char ExtraPadding[15];
		} FAT16; /* 39bytes */
		struct FAT32
		{ /* fat32 - 90 bytes */
			PACKED uint32_t BPB_FATSz32;
			PACKED uint16_t BPB_ExtFlags;
			PACKED uint16_t BPB_FSVer;
			PACKED uint32_t BPB_RootClus;
			PACKED uint16_t BPB_FSInfo;
			PACKED uint16_t BPB_BkBootSec;
			PACKED unsigned char BPB_Reserved[12];
			PACKED unsigned char BS_DrvNum;
			PACKED unsigned char BS_Reserved1;
			PACKED unsigned char BS_BootSig;
			PACKED uint32_t BS_VolID;
			PACKED char BS_VolLab[11];
			PACKED char BS_FilSysType[8];
		} FAT32; /* 54 */
	} BPB_EX;
}
FAT_BPB;	
END_PACKED_STRUCT
#if defined(USE_PRAGMA_PACK)
#pragma pack()
#endif

typedef struct FAT_QUERY_STATE_INTERNAL
{
	unsigned char Attributes;
	uint16_t current_sector;
	uint32_t current_cluster;
	FAT_RAW_DIRECTORY_ENTRY* current_entry_raw;
	unsigned char* buffer;

	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	uint16_t current_entry_raw_offset;
	FAT_RAW_DIRECTORY_ENTRY current_entry_raw_mem;
	#else
	FAT_RAW_DIRECTORY_ENTRY* first_entry_raw;
	#endif
	/*
	// LFN support members
	*/
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	uint16_t long_filename[256];
	unsigned char lfn_sequence;
	unsigned char lfn_checksum;
	#endif
}
FAT_QUERY_STATE_INTERNAL;


/*
// prototypes
*/
uint16_t fat_get_cluster_entry(FAT_VOLUME* volume, uint32_t cluster, FAT_ENTRY* fat_entry);
uint16_t fat_set_cluster_entry(FAT_VOLUME* volume, uint32_t cluster, FAT_ENTRY fat_entry);
uint16_t fat_free_cluster_chain(FAT_VOLUME* volume, uint32_t cluster);
uint32_t fat_allocate_data_cluster(FAT_VOLUME* volume, uint32_t count, char zero, uint16_t* result);
uint16_t fat_create_directory_entry(FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, char* name, unsigned char attribs, uint32_t entry_cluster, FAT_DIRECTORY_ENTRY* entry);
char fat_increase_cluster_address(FAT_VOLUME* volume, uint32_t current_cluster, uint16_t count, uint32_t* value);
char INLINE fat_is_eof_entry(FAT_VOLUME* volume, FAT_ENTRY fat);

unsigned char fat_long_entry_checksum( unsigned char* filename);
uint16_t get_short_name_for_entry(unsigned char* dest, unsigned char* src, char lfn_disabled);
uint32_t fat_allocate_directory_cluster(FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, uint16_t* result);
uint16_t fat_query_first_entry(FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* directory, unsigned char attributes, FAT_QUERY_STATE* query, char buffer_locked);
uint16_t fat_query_next_entry(FAT_VOLUME* volume, FAT_QUERY_STATE* query, char buffer_locked, char first_entry);
uint16_t fat_open_file_by_entry(FAT_VOLUME* volume, FAT_DIRECTORY_ENTRY* entry, FAT_FILE* handle, unsigned char access_flags);
uint16_t fat_file_read_internal(FAT_FILE* handle, unsigned char* buff, uint32_t length, uint32_t* bytes_read, uint16_t* state, FAT_ASYNC_CALLBACK* callback, void* callback_context);
void fat_file_read_callback(FAT_FILE* handle, uint16_t* async_state);
uint16_t fat_file_write_internal(FAT_FILE* handle, unsigned char* buff, uint32_t length, uint16_t* result, FAT_ASYNC_CALLBACK* callback, void* callback_context);	
void fat_file_write_callback(FAT_FILE* handle, uint16_t* async_state);
int INLINE indexof(char chr, char* str, int index);
	
void INLINE fat_get_short_name_from_entry(unsigned char* dest, const unsigned char* src);
char INLINE fat_compare_short_name(unsigned char* name1, unsigned char* name2);
unsigned char INLINE fat_get_fat32_sec_per_clus(uint32_t sector_count);
unsigned char INLINE fat_get_fat16_sec_per_clus(uint32_t sector_count);
void INLINE fat_fill_directory_entry_from_raw(FAT_DIRECTORY_ENTRY* entry, FAT_RAW_DIRECTORY_ENTRY* raw_entry);
uint16_t rtc_get_fat_date();
uint16_t rtc_get_fat_time();
INLINE time_t fat_decode_date_time(uint16_t date, uint16_t time);
INLINE void strtrim(char* dest, char* src, size_t max );
void fat_parse_path(char* path, char* path_part, char** filename_part);

#if defined(FAT_OPTIMIZE_FOR_FLASH)
uint32_t fat_allocate_data_cluster_ex(FAT_VOLUME* volume, uint32_t count, char zero, uint32_t page_size, uint16_t* result);
#endif

#if !defined(FAT_DISABLE_LONG_FILENAMES)
char INLINE fat_compare_long_name(uint16_t* name1, uint16_t* name2);
char INLINE get_long_name_for_entry(uint16_t* dst, unsigned char* src);
#endif

#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
void fat_read_partition_entry(FAT_PARTITION_ENTRY* entry, unsigned char* buffer);
void fat_read_bpb(FAT_BPB* bpb, unsigned char* buffer);
void fat_write_bpb(FAT_BPB* bpb, unsigned char* buffer);
void fat_read_raw_directory_entry(FAT_RAW_DIRECTORY_ENTRY* entry, unsigned char* buffer);
void fat_write_raw_directory_entry(FAT_RAW_DIRECTORY_ENTRY* entry, unsigned char* buffer);
void fat_read_fsinfo(FAT_FSINFO* fsinfo, unsigned char* buffer);
void fat_write_fsinfo(FAT_FSINFO* fsinfo, unsigned char* buffer);
#endif

#endif
