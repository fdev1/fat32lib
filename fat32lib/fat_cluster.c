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

#include "fat.h"
#include "fat_internals.h"
#include <stdlib.h>
#include <string.h>	/* for memset */

#define FAT_ALLOCATE_SEQUENTIAL_CLUSTERS

#if defined(FAT_ALLOCATE_SHARED_BUFFER)
#define FAT_IS_LOADED_SECTOR(sector)		(fat_shared_buffer_sector != 0xFFFFFFFF && fat_shared_buffer_sector == (sector))
#elif defined(FAT_ALLOCATE_VOLUME_BUFFER)
#define FAT_IS_LOADED_SECTOR(sector)		(volume->sector_buffer_sector != 0xFFFFFFFF && volume->sector_buffer_sector == (sector))
#else
#define FAT_IS_LOADED_SECTOR(sector)		(0)
#endif

#if defined(FAT_ALLOCATE_SHARED_BUFFER)
#define FAT_SET_LOADED_SECTOR(sector)		fat_shared_buffer_sector = (sector)
#elif defined(FAT_ALLOCATE_VOLUME_BUFFER)
#define FAT_SET_LOADED_SECTOR(sector)		volume->sector_buffer_sector = (sector)
#else
#define FAT_SET_LOADED_SECTOR(sector)	
#endif

/*
// macro for calculating the offset of a cluster entry within the FAT table
*/
#define FAT_CALCULATE_ENTRY_OFFSET(fs_type, cluster, offset)					\
	switch (fs_type)															\
	{																			\
		case FAT_FS_TYPE_FAT12: offset = cluster + (cluster >> 1); break;		\
		case FAT_FS_TYPE_FAT16: offset = cluster * (uint32_t) 2; break;					\
		case FAT_FS_TYPE_FAT32: offset = cluster * (uint32_t) 4; break;					\
	}

#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)		
#define FAT_LOCK_BUFFER()	\
	ENTER_CRITICAL_SECTION(volume->sector_buffer_lock)
#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)	
#define FAT_LOCK_BUFFER()	\
	ENTER_CRITICAL_SECTION(fat_shared_buffer_lock)
#else
#define FAT_LOCK_BUFFER()
#endif		

#if defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_VOLUME_BUFFER)
#define FAT_UNLOCK_BUFFER()	\
	LEAVE_CRITICAL_SECTION(volume->sector_buffer_lock)
#elif defined(FAT_MULTI_THREADED) && defined(FAT_ALLOCATE_SHARED_BUFFER)
#define FAT_UNLOCK_BUFFER()	\
	LEAVE_CRITICAL_SECTION(fat_shared_buffer_lock)
#else
#define FAT_UNLOCK_BUFFER()
#endif

#if defined(FAT_MULTI_THREADED)
#define FAT_ACQUIRE_READ_ACCESS()						\
	while (1)											\
	{													\
		ENTER_CRITICAL_SECTION(volume->write_lock);		\
		if (volume->readers_count < 0xFF)				\
		{												\
			volume->readers_count++;					\
			LEAVE_CRITICAL_SECTION(volume->write_lock);	\
			break;										\
		}												\
		LEAVE_CRITICAL_SECTION(volume->write_lock);		\
		RELINQUISH_THREAD();							\
	}
#define FAT_RELINQUISH_READ_ACCESS()					\
	volume->readers_count--

#define FAT_ACQUIRE_WRITE_ACCESS()						\
	ENTER_CRITICAL_SECTION(volume->write_lock);			\
	while (volume->readers_count)						\
		RELINQUISH_THREAD()
#define FAT_RELINQUISH_WRITE_ACCESS()					\
	LEAVE_CRITICAL_SECTION(volume->write_lock)
#else
#define FAT_ACQUIRE_READ_ACCESS()
#define FAT_RELINQUISH_READ_ACCESS()
#define FAT_ACQUIRE_WRITE_ACCESS()
#define FAT_RELINQUISH_WRITE_ACCESS()
#endif

/*
// prototypes for static functions
*/
static uint32_t INLINE fat_allocate_cluster(FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, uint32_t count, char zero, uint32_t page_size, uint16_t* result);
static uint16_t INLINE fat_initialize_directory_cluster(FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, uint32_t cluster, unsigned char* buffer);
static uint16_t INLINE fat_zero_cluster(FAT_VOLUME* volume, uint32_t cluster, unsigned char* buffer);
static INLINE void fat_write_fat_sector(FAT_VOLUME* volume, uint32_t sector_address, unsigned char* buffer, uint16_t* ret);

/*
// allocates a cluster for a directory - finds a free cluster, initializes it as
// required by a directory, marks it's FAT entry as EOC and returns the cluster address
//
// NOTE: this function used the volume/shared buffer (if enabled) so it must not be 
// locked before calling this function
*/
#if !defined(FAT_READ_ONLY)
uint32_t fat_allocate_directory_cluster(FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, uint16_t* result) 
{
	return fat_allocate_cluster(volume, parent, 1, 1, 1, result);
}	
#endif

/*
// allocates the requested number of clusters - finds the free clusters, initializes it to zeroes, 
// links them as a cluster chain (marking the last one as EOC) and returns the cluster number for
// the 1st cluster in the chain
//
// NOTE: this function used the volume/shared buffer (if enabled) so it must not be 
// locked before calling this function
*/
#if !defined(FAT_READ_ONLY)
uint32_t fat_allocate_data_cluster(FAT_VOLUME* volume, uint32_t count, char zero, uint16_t* result) 
{
	return fat_allocate_cluster(volume, 0, count, zero, 1, result);	
}
#endif

#if defined(FAT_OPTIMIZE_FOR_FLASH)
#if !defined(FAT_READ_ONLY)
uint32_t fat_allocate_data_cluster_ex(FAT_VOLUME* volume, uint32_t count, char zero, uint32_t page_size, uint16_t* result) 
{
	return fat_allocate_cluster(volume, 0, count, zero, page_size, result);	
}
#endif
#endif

/*
// calculate the gcd of 2 32 bit integers
*/
#if defined(FAT_OPTIMIZE_FOR_FLASH)
/*
static uint32_t fat_gcd(uint32_t a, uint32_t b)
{
	while(a != b) 
	{
		if (a > b)
		{
			a = a - b;
		}
		else
		{
			b = b - a;
		}
	}
	return a;
}
*/
#endif

/*
// performs the work for fat_allocate_data_cluster and fat_allocate_directory_cluster
//
// NOTE: this function used the volume/shared buffer (if enabled) so it must not be 
// locked before calling this function
*/
#if !defined(FAT_READ_ONLY)
static uint32_t INLINE fat_allocate_cluster(
	FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, uint32_t count, char zero, uint32_t page_size, uint16_t* result) 
{
	uint16_t ret = 0;				/* temp variable / stores return values from storage driver */
	uint32_t entry_sector;			/* the address of the cached sector */
	uint32_t current_sector;		/* address of the sector loaded in memory */
	uint32_t cluster;				/* cluster number */
	uint32_t entry_offset = 0;			/* offset of fat entry within it's sector */
	char entries_updated;			/* indicates that the cached sector is dirty */
	char next_sector_loaded = 0;	/* indicates that the next sector has been loaded */
	FAT_ENTRY last_fat_entry = 0;		/* stores the value of the last cluster found or EOC if no clusters found yet */
	FAT_ENTRY fat_entry;			/* temp value to store cluster numbers read from FAT table */
	uint32_t first_cluster;
	uint32_t last_entry_sector = 0;
	uint32_t last_entry_offset = 0;
	uint32_t start_cluster;
	char wrapped_around = 0;

	#if defined(FAT_OPTIMIZE_FOR_FLASH)
	uint16_t step = 1;
	#endif

	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	unsigned char* buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	unsigned char* buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	#endif
	/*
	// the zero and parent parameters should only be set when
	// allocating only 1 cluster. Also this function should never
	// be called with count = 0 as it won't return a cluster # which
	// will cause undefined behaviour so the caller needs to make sure
	// that it's requesting at least 1 cluster.
	*/
	_ASSERT(count > 0);
	_ASSERT(!(count > 1 && (parent != 0 || zero != 0)));
	
	if (!count)
		return FAT_UNKNOWN_ERROR;
	/*
	// init values
	*/
	fat_entry = 0;
	entries_updated = 0;
	*result = FAT_SUCCESS;
	cluster = 0x2;
	first_cluster = 0;
	/*
	// if we got a hint of the 1st free cluster then take it
	*/
	if (volume->next_free_cluster != 0xFFFFFFFF)
	{
		if (volume->next_free_cluster <= volume->no_of_clusters + 1)
		{
			cluster = volume->next_free_cluster;
		}
	}
	/*
	// find the step between clusters allocated on page boundaries
	*/
	#if defined(FAT_OPTIMIZE_FOR_FLASH)
	if (volume->no_of_sectors_per_cluster < page_size)
	{
		uint32_t sector;
		uint16_t step_count = 0;
		step = (uint16_t) (page_size / volume->no_of_sectors_per_cluster);
		/*
		// find the 1st cluster that starts on a page boundary
		*/
		sector = FIRST_SECTOR_OF_CLUSTER(volume, cluster);
		while (sector % page_size && step_count < step)
		{
			cluster++;
			step_count++;
			sector = FIRST_SECTOR_OF_CLUSTER(volume, cluster);
		}
	}
	#endif
	/*
	// remember the 1st cluster of our search
	*/
	start_cluster = cluster;
	/*
	// set the last_fat_entry value to the eof marker
	*/
	switch (volume->fs_type) 
	{
		case FAT_FS_TYPE_FAT12 : last_fat_entry = FAT12_EOC; break;
		case FAT_FS_TYPE_FAT16 : last_fat_entry = FAT16_EOC; break;
		case FAT_FS_TYPE_FAT32 : last_fat_entry = FAT32_EOC; break;
	}
	/*
	// acquire buffer lock
	*/
	FAT_ACQUIRE_WRITE_ACCESS();
	FAT_LOCK_BUFFER();
	/*
	// calculate the offset of the FAT entry within it's sector
	// and the sector number
	*/
	FAT_CALCULATE_ENTRY_OFFSET(volume->fs_type, cluster, entry_offset); 
	entry_sector = volume->no_of_reserved_sectors + (entry_offset / volume->no_of_bytes_per_serctor);
	entry_offset = entry_offset % volume->no_of_bytes_per_serctor; 
	last_entry_sector = entry_sector;
	/*
	// for each sector of the FAT
	*/
	while (1) 
	{
		/*
		// read sector into sector cache
		*/
		if (!FAT_IS_LOADED_SECTOR(entry_sector))
		{
			ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
			if (ret != STORAGE_SUCCESS)
			{
				FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_WRITE_ACCESS();
				*result = FAT_CANNOT_READ_MEDIA;
				return 0;
			}
			FAT_SET_LOADED_SECTOR(entry_sector);
		}
		/*
		// save the current sector number
		*/
		current_sector = entry_sector;
		/*
		// for every FAT entry in the sector...
		*/
		do
		{
			/*
			// if we've reached the last cluster on the drive return insufficient
			// disk space error code.
			*/
			if ((cluster > volume->no_of_clusters + 1) && start_cluster > 2 && !wrapped_around)
			{
				if (entries_updated)
				{
					/*
					// if the buffer is dirty flush the changes
					*/
					fat_write_fat_sector(volume, current_sector, buffer, &ret);
					if (ret != STORAGE_SUCCESS)
					{
						FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
						FAT_UNLOCK_BUFFER();
						FAT_RELINQUISH_WRITE_ACCESS();
						*result = FAT_CANNOT_READ_MEDIA;
						return 0;
					}
					/*
					// mark the buffer as clean
					*/
					entries_updated = 0;
				}
				/*
				// reset the cluster # to 2
				*/
				cluster = 2;
				wrapped_around = 1;
				/*
				// calculate the sector for the new cluster
				*/
				FAT_CALCULATE_ENTRY_OFFSET(volume->fs_type, cluster, entry_offset);
				entry_sector = volume->no_of_reserved_sectors;
				entry_offset = entry_offset % volume->no_of_bytes_per_serctor;
				/*
				// break from this loop so that sector gets loaded
				*/
				break;

			}
			else if ((cluster > volume->no_of_clusters + 1) || (wrapped_around && cluster >= start_cluster))
			{
				if (entries_updated)
				{
					/*
					// if the buffer is dirty flush the changes, otherwise when we try
					// to free the cluster chain it would run into free clusters before
					// the EOC resulting in lost chains (or a hang in a debug build).
					*/
					fat_write_fat_sector(volume, current_sector, buffer, &ret);
					if (ret != STORAGE_SUCCESS)
					{
						FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
						FAT_UNLOCK_BUFFER();
						FAT_RELINQUISH_WRITE_ACCESS();
						*result = FAT_CANNOT_READ_MEDIA;
						return 0;
					}
					/* entries_updated = 0; */
				}
				/*
				// release the lock on the buffer
				*/
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_WRITE_ACCESS();
				/*
				// free any clusters that we've allocated so far
				*/
				if (!fat_is_eof_entry(volume, last_fat_entry))
				{
					/*
					// even if this returns error we cannot do anything with
					// it as we're already returning an error code so we can only
					// hope that the clusters get freed.
					*/
					ret = fat_free_cluster_chain(volume, last_fat_entry);
				}
				/*
				// return insufficient disk space error
				*/
				*result = FAT_INSUFFICIENT_DISK_SPACE;
				return 0;
			}
			/*
			// copy the next FAT entry to the fat_entry variable
			*/
			switch (volume->fs_type)
			{
				case FAT_FS_TYPE_FAT12:
				{
					/*
					// clear fat_entry to make sure that the upper 16
					// bits are not set.
					*/
					fat_entry = 0;	
					/*
					// read the 1st byte
					*/
					((unsigned char*) &fat_entry)[INT32_BYTE0] = buffer[entry_offset];
					/*
					// load the next sector (if necessary) and set the offset
					// for the next byte in the buffer
					*/
					if (entry_offset == volume->no_of_bytes_per_serctor - 1)
					{
						if (entries_updated)
						{
							/*
							// if the buffer is dirty flush it before we use it to load
							// the next sector
							*/
							fat_write_fat_sector(volume, entry_sector, buffer, &ret);
							if (ret != STORAGE_SUCCESS)
							{
								FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
								FAT_UNLOCK_BUFFER();
								FAT_RELINQUISH_WRITE_ACCESS();
								*result = FAT_CANNOT_READ_MEDIA;
								return 0;
							}
							/*
							// mark the buffer as clean
							*/
							entries_updated = 0;
						}
						/*
						// load the next sector into the buffer
						*/
						ret = volume->device->read_sector(volume->device->driver, entry_sector + 1, buffer);
						if (ret != STORAGE_SUCCESS)
						{
							FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
							FAT_UNLOCK_BUFFER();
							FAT_RELINQUISH_WRITE_ACCESS();
							*result = FAT_CANNOT_READ_MEDIA;
							return 0;
						}
						/*
						// set flag to indicate that we had to load the next sector
						*/
						next_sector_loaded = 1;
					}
					/*
					// read the 2nd byte
					*/
					((unsigned char*) &fat_entry)[INT32_BYTE1] = buffer[next_sector_loaded ? 0 : (entry_offset + 1)];
					/*
					// Since a FAT12 entry is only 12 bits (1.5 bytes) we need to adjust the result.
					// For odd cluster numbers the FAT entry is stored in the upper 12 bits of the
					// 16 bits where it is stored, so we need to shift the value 4 bits to the right.
					// For even cluster numbers the FAT entry is stored in the lower 12 bits of the
					// 16 bits where it is stored, so we need to clear the upper 4 bits.
					*/
					if (cluster & 0x1)
					{
						fat_entry >>= 4;
					}
					else
					{
						fat_entry &= 0xFFF;
					}
					/*
					// if we had to load the next sector into the cache we need
					// to reload the current sector before continuing
					*/
					if (next_sector_loaded)
					{
						ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
						if (ret != STORAGE_SUCCESS)
						{
							FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
							FAT_UNLOCK_BUFFER();
							FAT_RELINQUISH_WRITE_ACCESS();
							*result = FAT_CANNOT_READ_MEDIA;
							return 0;
						}
						next_sector_loaded = 0;
					}
					break;
				}
				case FAT_FS_TYPE_FAT16:
				{
					#if defined(BIG_ENDIAN)
					((unsigned char*) &fat_entry)[INT32_BYTE0] = buffer[entry_offset + 0];
					((unsigned char*) &fat_entry)[INT32_BYTE1] = buffer[entry_offset + 1];
					((unsigned char*) &fat_entry)[INT32_BYTE2] = 0;
					((unsigned char*) &fat_entry)[INT32_BYTE3] = 0;
					#else
					fat_entry = *((uint16_t*) &buffer[entry_offset]);
					#endif
					break;
				}
				case FAT_FS_TYPE_FAT32:
				{
					#if defined(BIG_ENDIAN)
					((unsigned char*) &fat_entry)[INT32_BYTE0] = buffer[entry_offset + 0];
					((unsigned char*) &fat_entry)[INT32_BYTE1] = buffer[entry_offset + 1];
					((unsigned char*) &fat_entry)[INT32_BYTE2] = buffer[entry_offset + 2];
					((unsigned char*) &fat_entry)[INT32_BYTE3] = buffer[entry_offset + 3] & 0x0F;
					#else
					fat_entry = *((uint32_t*) &buffer[entry_offset]) & 0x0FFFFFFF;
					#endif
					break;
				}
			}
			/*
			// if the current FAT is free
			*/
			if (IS_FREE_FAT(volume, fat_entry)) 
			{
				/*
				// maintain the count of free cluster and the next
				// cluster that may be free
				*/
				volume->next_free_cluster = cluster + 1;
				volume->total_free_clusters--;
				/*
				// if this is the 1st cluster found remember it
				*/
				if (!first_cluster)
					first_cluster = cluster;
				/*
				// mark the FAT as the the new 1st link of the cluster chain
				// (or the end of the chain if we're only allocating 1 cluster)
				*/
				switch (volume->fs_type)
				{
					case FAT_FS_TYPE_FAT12:
					{
						/*
						// write the 1st byte
						*/
						if (cluster & 0x1)
						{
							buffer[entry_offset] &= 0x0F;								/* clear entry bits on 1st byte */
							buffer[entry_offset] |= LO8((uint16_t) FAT12_EOC << 4);		/* set entry bits on 1st byte */
						}
						else
						{
							buffer[entry_offset] = LO8((uint16_t) FAT12_EOC);			/* just copy the 1st byte */
						}
						/*
						// if the FAT entry spans a sector boundary flush the currently
						// loaded sector to the drive and load the next one.
						*/
						if (entry_offset == volume->no_of_bytes_per_serctor - 1)
						{
							/*
							// flush the updated sector to the drive
							*/
							fat_write_fat_sector(volume, entry_sector, buffer, &ret);
							if (ret != STORAGE_SUCCESS) 
							{
								FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
								FAT_UNLOCK_BUFFER();
								FAT_RELINQUISH_WRITE_ACCESS();
								*result = FAT_CANNOT_WRITE_MEDIA;
								return 0;
							}
							/*
							// mark the buffer as clean
							*/
							entries_updated = 0;
							/*
							// load the next sector
							*/
							ret = volume->device->read_sector(volume->device->driver, entry_sector + 1, buffer);
							if (ret != STORAGE_SUCCESS)
							{
								FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
								FAT_UNLOCK_BUFFER();
								FAT_RELINQUISH_WRITE_ACCESS();
								*result = FAT_CANNOT_READ_MEDIA;
								return 0;
							}
							/*
							// set the next_sector_loaded flag
							*/
							next_sector_loaded = 1;
						}
						/*
						// write the 2nd byte
						*/
						if (cluster & 0x1)
						{
							buffer[next_sector_loaded ? 0 : (entry_offset + 1)] = HI8((uint16_t) FAT12_EOC << 4);	
						}
						else
						{
							buffer[next_sector_loaded ? 0 : (entry_offset + 1)] &= 0xF0;							
							buffer[next_sector_loaded ? 0 : (entry_offset + 1)] |= HI8((uint16_t) FAT12_EOC);	
						}
						/*
						// if the next sector has been loaded then flush it
						*/
						if (next_sector_loaded)
						{
							/*
							// flush buffer to disk
							*/
							fat_write_fat_sector(volume, entry_sector + 1, buffer, &ret);
							if (ret != STORAGE_SUCCESS)
							{
								FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
								FAT_UNLOCK_BUFFER();
								FAT_RELINQUISH_WRITE_ACCESS();
								*result = FAT_CANNOT_READ_MEDIA;
								return 0;
							}
							/*
							// note: we don't reload the current sector because this being the last entry
							// the next sector is about to be reloaded anyways
							*/
							next_sector_loaded = 0;
							/*
							// reload the current sector
							*/
							ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
							if (ret != STORAGE_SUCCESS)
							{
								FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
								FAT_UNLOCK_BUFFER();
								FAT_RELINQUISH_WRITE_ACCESS();
								*result = FAT_CANNOT_READ_MEDIA;
								return 0;
							}
							entries_updated = 0;
						}
						else
						{
							entries_updated = 1;
						}
						/*
						// if this is not the 1st cluster allocated update
						// the last one to link to this one
						*/
						if (last_fat_entry != FAT12_EOC)
						{
							if (last_entry_sector != entry_sector)
							{
								if (entries_updated)
								{
									/*
									// flush buffer to disk
									*/
									fat_write_fat_sector(volume, entry_sector, buffer, &ret);
									if (ret != STORAGE_SUCCESS)
									{
										FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
										FAT_UNLOCK_BUFFER();
										FAT_RELINQUISH_WRITE_ACCESS();
										*result = FAT_CANNOT_READ_MEDIA;
										return 0;
									}
									/*
									// mark buffer as clean
									*/
									entries_updated = 0;
								}
								/*
								// load last_entry sector
								*/
								ret = volume->device->read_sector(volume->device->driver, last_entry_sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
							}
							/*
							// update the last entry to point to this one
							*/
							if (last_fat_entry & 0x1)
							{
								buffer[last_entry_offset] &= 0x0F;							/* clear entry bits on 1st byte */
								buffer[last_entry_offset] |= LO8((uint16_t) cluster << 4);	/* set entry bits on 1st byte */
							}
							else
							{
								buffer[last_entry_offset] = LO8((uint16_t) cluster);	/* just copy the 1st byte */
							}
							/*
							// if the FAT entry spans a sector boundary flush the currently
							// loaded sector to the drive and load the next one.
							*/
							if (last_entry_offset == volume->no_of_bytes_per_serctor - 1)
							{
								/*
								// flush the updated sector to the drive
								*/
								fat_write_fat_sector(volume, last_entry_sector, buffer, &ret);
								if (ret != STORAGE_SUCCESS) 
								{
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_WRITE_MEDIA;
									return 0;
								}
								/*
								// mark the buffer as clean
								*/
								entries_updated = 0;
								/*
								// load the next sector
								*/
								ret = volume->device->read_sector(volume->device->driver, last_entry_sector + 1, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
								/*
								// set the next_sector_loaded flag
								*/
								next_sector_loaded = 1;
							}
							/*
							// write the 2nd byte
							*/
							if (last_fat_entry & 0x1)
							{
								buffer[next_sector_loaded ? 0 : (last_entry_offset + 1)] = HI8((uint16_t) cluster << 4);	
							}
							else
							{
								buffer[next_sector_loaded ? 0 : (last_entry_offset + 1)] &= 0xF0;							
								buffer[next_sector_loaded ? 0 : (last_entry_offset + 1)] |= HI8((uint16_t) cluster);	
							}
							/*
							// if the next sector has been loaded then flush it
							*/
							if (next_sector_loaded)
							{
								/*
								// flush buffer to disk
								*/
								fat_write_fat_sector(volume, last_entry_sector + 1, buffer, &ret);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
								/*
								// note: we don't reload the current sector because this being the last entry
								// the next sector is about to be reloaded anyways
								*/
								next_sector_loaded = 0;
								/*
								// reload the last entry sector
								*/
								ret = volume->device->read_sector(volume->device->driver, last_entry_sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
							}
							else
							{
								entries_updated = 1;
							}
							if (last_entry_sector != entry_sector)
							{
								if (entries_updated)
								{
									/*
									// flush buffer to disk
									*/
									fat_write_fat_sector(volume, last_entry_sector, buffer, &ret);
									if (ret != STORAGE_SUCCESS)
									{
										FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
										FAT_UNLOCK_BUFFER();
										FAT_RELINQUISH_WRITE_ACCESS();
										*result = FAT_CANNOT_READ_MEDIA;
										return 0;
									}
									/*
									// mark buffer as clean
									*/
									entries_updated = 0;
								}
								/*
								// reload current sector
								*/
								ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
							}
						}
						break;
					}
					case FAT_FS_TYPE_FAT16:
					{
						#if defined(BIG_ENDIAN)
						buffer[entry_offset + 0] = LO8(FAT16_EOC);
						buffer[entry_offset + 1] = HI8(FAT16_EOC);
						#else
						*((uint16_t*) &buffer[entry_offset]) = FAT16_EOC;
						#endif
						/*
						// mark the buffer as dirty
						*/
						entries_updated = 1;
						/*
						// if this is not the first cluster allocated update
						// the last one to link to this one
						*/
						if (last_fat_entry != FAT16_EOC)
						{
							if (last_entry_sector != entry_sector)
							{
								/*
								// flush buffer to disk
								*/
								fat_write_fat_sector(volume, entry_sector, buffer, &ret);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
								/*
								// mark buffer as clean
								*/
								entries_updated = 0;
								/*
								// load last_entry sector
								*/
								ret = volume->device->read_sector(volume->device->driver, last_entry_sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
							}
							/*
							// update the last entry to point to this one
							*/
							#if defined(BIG_ENDIAN)
							buffer[last_entry_offset + 0] = ((unsigned char*) &cluster)[INT32_BYTE0];
							buffer[last_entry_offset + 1] = ((unsigned char*) &cluster)[INT32_BYTE1];
							#else
							*((uint16_t*) &buffer[last_entry_offset]) = (uint16_t) cluster;
							#endif

							if (last_entry_sector != entry_sector)
							{
								/*
								// flush buffer to disk
								*/
								fat_write_fat_sector(volume, last_entry_sector, buffer, &ret);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
								/*
								// reload current sector
								*/
								ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
							}
						}
						break;
					}
					case FAT_FS_TYPE_FAT32:
					{
						#if defined(BIG_ENDIAN)
						buffer[entry_offset + 0] = LO8(LO16(FAT32_EOC));
						buffer[entry_offset + 1] = HI8(LO16(FAT32_EOC));
						buffer[entry_offset + 2] = LO8(HI16(FAT32_EOC));
						buffer[entry_offset + 3] &= 0xF0;
						buffer[entry_offset + 3] |= HI8(HI16(FAT32_EOC)) & 0x0F;
						#else
						*((uint32_t*) &buffer[entry_offset]) &= 0xF0000000;
						*((uint32_t*) &buffer[entry_offset]) |= FAT32_EOC & 0x0FFFFFFF;
						#endif
						/*
						// mark the buffer as dity
						*/
						entries_updated = 1;
						/*
						// if this is not the 1st cluster allocated update
						// the last one to link to this one
						*/
						if (last_fat_entry != FAT32_EOC)
						{
							if (last_entry_sector != entry_sector)
							{
								/*
								// flush buffer to disk
								*/
								fat_write_fat_sector(volume, entry_sector, buffer, &ret);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
								/*
								// mark buffer as clean
								*/
								entries_updated = 0;
								/*
								// load last_entry sector
								*/
								ret = volume->device->read_sector(volume->device->driver, last_entry_sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
							}
							/*
							// update the last entry to point to this one
							*/
							#if defined(BIG_ENDIAN)
							buffer[last_entry_offset + 0] = ((unsigned char*) &cluster)[INT32_BYTE0];
							buffer[last_entry_offset + 1] = ((unsigned char*) &cluster)[INT32_BYTE1];
							buffer[last_entry_offset + 2] = ((unsigned char*) &cluster)[INT32_BYTE2];
							buffer[last_entry_offset + 3] &= 0xF0;
							buffer[last_entry_offset + 3] |= ((unsigned char*) &cluster)[INT32_BYTE3] & 0x0F;
							#else
							*((uint32_t*) &buffer[last_entry_offset]) &= 0xF0000000;
							*((uint32_t*) &buffer[last_entry_offset]) |= cluster & 0x0FFFFFFF;
							#endif

							if (last_entry_sector != entry_sector)
							{
								/*
								// flush buffer to disk
								*/
								fat_write_fat_sector(volume, last_entry_sector, buffer, &ret);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
								/*
								// reload current sector
								*/
								ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
								if (ret != STORAGE_SUCCESS)
								{
									FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
									FAT_UNLOCK_BUFFER();
									FAT_RELINQUISH_WRITE_ACCESS();
									*result = FAT_CANNOT_READ_MEDIA;
									return 0;
								}
							}
						}
						break;
					}
				}
				/*
				// if we've found all the clusters that the user requested leave
				// and return cluster #
				*/
				if (!--count)
				{
					if (entries_updated)
					{
						/*
						// flush buffer to disk
						*/
						fat_write_fat_sector(volume, entry_sector, buffer, &ret);
						if (ret != STORAGE_SUCCESS)
						{
							FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
							FAT_UNLOCK_BUFFER();
							FAT_RELINQUISH_WRITE_ACCESS();
							*result = FAT_CANNOT_READ_MEDIA;
							return 0;
						}
					}
					/*
					// if we're allocating a cluster for a
					// directory then initialize it properly
					*/
					if (parent) 
					{
						ret = fat_initialize_directory_cluster(volume, parent, cluster, buffer);
						if (ret != FAT_SUCCESS) 
						{
							FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
							FAT_UNLOCK_BUFFER();
							FAT_RELINQUISH_WRITE_ACCESS();
							*result = ret;
							return 0;
						}
					}
					else
					{
						if (zero)
						{
							ret = fat_zero_cluster(volume, cluster, buffer);
							if (ret != FAT_SUCCESS)
							{
								FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
								FAT_UNLOCK_BUFFER();
								FAT_RELINQUISH_WRITE_ACCESS();
								*result = ret;
								return 0;
							}
						}
					}
					FAT_UNLOCK_BUFFER();
					FAT_RELINQUISH_WRITE_ACCESS();
					return first_cluster;
				}
				/*
				// remember the cluster number so we can mark the
				// next fat entry with it
				*/
				last_fat_entry = cluster;
				last_entry_sector = entry_sector;
				last_entry_offset = entry_offset;
			}
			/*
			// increase the cluster number
			*/
			#if defined(FAT_OPTIMIZE_FOR_FLASH)
			if (!first_cluster)
			{
				cluster += step;
			}
			else
			{
				cluster++;
			}
			#else
			cluster++;
			#endif
			/*
			// calculate the offset of the cluster's FAT entry within it's sector
			// note: when we hit get past the end of the current sector entry_offset
			// will roll back to zero (or possibly 1 for FAT12)
			*/
			FAT_CALCULATE_ENTRY_OFFSET(volume->fs_type, cluster, entry_offset); 
			entry_sector = volume->no_of_reserved_sectors + (entry_offset / volume->no_of_bytes_per_serctor);
			entry_offset = entry_offset % volume->no_of_bytes_per_serctor; 

		}
		while (current_sector == entry_sector);
		/*
		// if any changes where made to the fat entry currently cached sector
		// flush the changes to the drive before moving to next sector
		*/
		if (entries_updated)
		{
			fat_write_fat_sector(volume, current_sector, buffer, &ret);
			if (ret != STORAGE_SUCCESS)
			{
				FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_WRITE_ACCESS();
				*result = FAT_CANNOT_READ_MEDIA;
				return 0;
			}
			entries_updated = 0;
		}
	}
}
#endif

/*
// marks all the clusters in the cluster chain as free
*/
#if !defined(FAT_READ_ONLY)
uint16_t fat_free_cluster_chain(FAT_VOLUME* volume, uint32_t cluster) 
{
	uint16_t ret;				/* temp variable / used to hold return codes from driver */
	uint32_t fat_offset = 0;		/* the offset of the cluster entry within the FAT table */
	uint32_t entry_offset;		/* the offset of the cluster entry within it's sector */
	uint32_t entry_sector;		/* the sector where the entry is stored on the drive */
	uint32_t current_sector;	/* the sector that's currently loaded in memory */
	char is_odd_cluster = 0;		/* indicates that the entry being processed is an odd cluster address (FAT12 only) */
	char op_in_progress = 0;	/* indicates that a multi-step operation is in progress (FAT12 only) */

	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	unsigned char* buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	unsigned char* buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	#endif
	/*
	// get the offset of the cluster entry within the FAT table,
	// the sector of the FAT table that contains the entry and the offset
	// of the fat entry within the sector
	*/				
	FAT_CALCULATE_ENTRY_OFFSET(volume->fs_type, cluster, fat_offset);
	entry_sector = volume->no_of_reserved_sectors + (fat_offset / volume->no_of_bytes_per_serctor);
	entry_offset = fat_offset % volume->no_of_bytes_per_serctor;
	/*
	// acquire lock on buffer
	*/
	FAT_ACQUIRE_WRITE_ACCESS();
	FAT_LOCK_BUFFER();
	/*
	// loop until we reach the EOC cluster or an error occurs.
	*/
	while (1)
	{
		/*
		// load sector to memory
		*/
		if (!FAT_IS_LOADED_SECTOR(entry_sector))
		{
			ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
			if (ret != STORAGE_SUCCESS)
			{
				FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_WRITE_ACCESS();
				return FAT_CANNOT_READ_MEDIA;
			}
			FAT_SET_LOADED_SECTOR(entry_sector);
		}
		/*
		// store the address of the sector that's (will be) loaded in memory
		*/
		current_sector = entry_sector;
		/*
		// loop until a new sector needs to be loaded to continue
		// with the operation
		*/
		while (current_sector == entry_sector)
		{
			/*
			// if cluster is less than 2 either we got a bug
			// or the file system is corrupted
			*/
			_ASSERT(cluster >= 2);
			/*
			// make sure we don't try to free an invalid cluster
			*/
			if (cluster < 2)
			{
				/*
				// leave critical section and return error code
				*/
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_WRITE_ACCESS();
				return FAT_INVALID_CLUSTER;
			}
			/*
			// read the cluster entry and mark it as free
			*/
			switch (volume->fs_type)
			{
				case FAT_FS_TYPE_FAT12:
				{
					if (!op_in_progress)
					{
						/*
						// remember whether this is an odd cluster or not
						*/
						is_odd_cluster = (cluster & 0x1);
						/*
						// set the cluster to zero to make sure that the upper bytes are cleared
						// since we're only updating the lower 16 bits. 
						*/
						cluster = 0;
						/*
						// read the 1st byte
						*/
						((unsigned char*) &cluster)[0] = buffer[entry_offset];
						/*
						// write the 1st byte
						//
						// note: since the value that we're writting is FREE_FAT which expands
						// to zero we can skip a step or two in the update process. I left the code
						// for the steps that we're skipping commented out for clarity.
						*/
						if (is_odd_cluster)
						{
							buffer[entry_offset] &= 0x0F;									/* clear entry bits on 1st byte */
							/* buffer[entry_offset] |= LO8((uint16_t) FREE_FAT << 4); */	/* set entry bits on 1st byte */
						}
						else
						{
							/* buffer[entry_offset] = LO8((uint16_t) FREE_FAT);	*/			/* just copy the 1st byte */
							buffer[entry_offset] = FREE_FAT;
						}
					}

					if (entry_offset == volume->no_of_bytes_per_serctor - 1)
					{
						/*
						// flush current sector to drive
						*/
						fat_write_fat_sector(volume, current_sector, buffer, &ret);
						if (ret != STORAGE_SUCCESS)
						{
							FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
							FAT_UNLOCK_BUFFER();
							FAT_RELINQUISH_WRITE_ACCESS();
							return FAT_CANNOT_READ_MEDIA;
						}
						/*
						// if the entry spans a sector boundary set op_in_progress to 1
						// so that we don't read the 1st byte again when we come back.
						// also increase the sector number and set the entry_offset to 0 since
						// the next byte will be on offset zero when the next sector is loaded
						*/
						entry_sector++;
						entry_offset = 0;
						op_in_progress = 1;
						/*
						// continue with the next iteration of the loop. We'll come right back
						// here with the next sector loaded
						*/
						continue;
					}
					else if (!op_in_progress)
					{
						/*
						// increase the offset to point to the next byte
						*/
						entry_offset++;
					}
					/*
					// read the 2nd byte
					*/
					((unsigned char*) &cluster)[1] = buffer[entry_offset];
					/*
					// Since a FAT12 entry is only 12 bits (1.5 bytes) we need to adjust the result.
					// For odd cluster numbers the FAT entry is stored in the upper 12 bits of the
					// 16 bits where it is stored, so we need to shift the value 4 bits to the right.
					// For even cluster numbers the FAT entry is stored in the lower 12 bits of the
					// 16 bits where it is stored, so we need to clear the upper 4 bits.
					*/
					if (is_odd_cluster)
					{
						cluster >>= 4;
					}
					else
					{
						cluster &= 0xFFF;
					}
					/*
					// write the 2nd byte
					//
					// note: since the value that we're writting is FREE_FAT which expands
					// to zero we can skip a step or two in the update process. I left the code
					// for the steps that we're skipping commented out for clarity.
					//
					// note: we could bundle this with the above if...else statement but
					// since that is part of the read operation and this is the write operation
					// i've decided to keep it separate for code clarity. maybe the compiler
					// can optimize this for us??
					*/
					if (is_odd_cluster)
					{
						/* buffer[entry_offset] = HI8((uint16_t) FREE_FAT << 4); */		/* just copy the 1st byte */
						buffer[entry_offset] = FREE_FAT;
					}
					else
					{
						buffer[entry_offset] &= 0xF0;									/* clear bits that 1st byte will be written to */
						/*buffer[entry_offset] |= HI8((uint16_t) FREE_FAT);	*/			/* copy entry bits of 1st byte */
					}
					/*
					// clear op_in_progress flag.
					*/
					op_in_progress = 0;
					break;
				}
				case FAT_FS_TYPE_FAT16:
				{
					#if defined(BIG_ENDIAN)
					((unsigned char*) &cluster)[INT32_BYTE0] = buffer[entry_offset + 0];
					((unsigned char*) &cluster)[INT32_BYTE1] = buffer[entry_offset + 1];
					((unsigned char*) &cluster)[INT32_BYTE2] = 0;
					((unsigned char*) &cluster)[INT32_BYTE3] = 0;
					buffer[entry_offset + 0] = 0;
					buffer[entry_offset + 1] = 0;
					#else
					cluster = (uint32_t) *((uint16_t*) &buffer[entry_offset]);
					*((uint16_t*) &buffer[entry_offset]) = FREE_FAT;
					#endif
					break;
				}
				case FAT_FS_TYPE_FAT32:
				{
					/*
					// FAT32 entries are actually 28 bits so we need to leave the
					// upper nibble untouched
					*/
					#if defined(BIG_ENDIAN)
					((unsigned char*) &cluster)[INT32_BYTE0] = buffer[entry_offset + 0];
					((unsigned char*) &cluster)[INT32_BYTE1] = buffer[entry_offset + 1];
					((unsigned char*) &cluster)[INT32_BYTE2] = buffer[entry_offset + 2];
					((unsigned char*) &cluster)[INT32_BYTE3] = buffer[entry_offset + 3] & 0x0F;
					buffer[entry_offset + 0] = 0;
					buffer[entry_offset + 1] = 0;
					buffer[entry_offset + 2] = 0;
					buffer[entry_offset + 3] &= 0xF0;
					#else
					cluster = *((uint32_t*) &buffer[entry_offset]) & 0x0FFFFFFF;
					*((uint32_t*) &buffer[entry_offset]) &= 0xF0000000;
					/* *((uint32_t*) &buffer[entry_offset]) |= FREE_FAT; */
					#endif
					break;
				}
			}
			/*
			// increase the count of free clusters
			*/
			volume->total_free_clusters++;
			/*
			// if it's the EOF marker we're done, flush the buffer and go
			*/
			if (fat_is_eof_entry(volume, cluster))
			{
				fat_write_fat_sector(volume, current_sector, buffer, &ret);
				if (ret != STORAGE_SUCCESS)
				{
					FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
					FAT_UNLOCK_BUFFER();
					FAT_RELINQUISH_WRITE_ACCESS();
					return FAT_CANNOT_READ_MEDIA;
				}
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_WRITE_ACCESS();
				return FAT_SUCCESS;
			}
			/*
			// calculate the location of the next cluster in the chain
			*/
			FAT_CALCULATE_ENTRY_OFFSET(volume->fs_type, cluster, fat_offset);
			entry_sector = volume->no_of_reserved_sectors + (fat_offset / volume->no_of_bytes_per_serctor);
			entry_offset = fat_offset % volume->no_of_bytes_per_serctor;
		}
		/*
		// flush FAT table changes
		*/
		fat_write_fat_sector(volume, current_sector, buffer, &ret);
		if (ret != STORAGE_SUCCESS)
		{
			FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
			FAT_UNLOCK_BUFFER();
			FAT_RELINQUISH_WRITE_ACCESS();
			return FAT_CANNOT_READ_MEDIA;
		}
	}
}	
#endif

/*
// gets the FAT structure for a given cluster number
*/
uint16_t fat_get_cluster_entry( 
	FAT_VOLUME* volume, uint32_t cluster, FAT_ENTRY* fat_entry ) 
{
	uint16_t ret;
	uint32_t fat_offset = 0;	/* todo: this one may require 64 bits for large drives? */
	uint32_t entry_sector;	
	uint32_t entry_offset;	/* todo: 16 bits should suffice for this value */

	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	unsigned char* buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	unsigned char* buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	#endif
	/*
	// get the offset of the entry within the FAT table 
	// for the requested cluster
	*/
	switch (volume->fs_type)
	{
		case FAT_FS_TYPE_FAT12: fat_offset = cluster + (cluster >> 1); 	break;
		case FAT_FS_TYPE_FAT16: fat_offset = cluster * ((uint32_t) 2); break;
		case FAT_FS_TYPE_FAT32: fat_offset = cluster * ((uint32_t) 4); break;
	}
	/*
	// get the address of the sector that contains the FAT entry and 
	// the offset of the FAT entry within that sector
	*/
	entry_sector = volume->no_of_reserved_sectors + (fat_offset / volume->no_of_bytes_per_serctor);
	entry_offset = fat_offset % volume->no_of_bytes_per_serctor;
	/*
	// acquire lock on buffer
	*/
	FAT_ACQUIRE_READ_ACCESS();
	FAT_LOCK_BUFFER();
	/*
	// load sector into the buffer
	*/
	if (!FAT_IS_LOADED_SECTOR(entry_sector))
	{
		ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
		if (ret != STORAGE_SUCCESS)
		{
			FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
			FAT_UNLOCK_BUFFER();
			FAT_RELINQUISH_READ_ACCESS();
			return FAT_CANNOT_READ_MEDIA;
		}
		FAT_SET_LOADED_SECTOR(entry_sector);
	}
	/*
	// set the user supplied buffer with the
	// value of the FAT entry
	*/
	switch (volume->fs_type)
	{
		case FAT_FS_TYPE_FAT12:
		{
			/*
			// clear fat_entry to make sure that the upper 16
			// bits are not set.
			*/
			*fat_entry = 0;	
			/*
			// read the 1st byte
			*/
			((unsigned char*) fat_entry)[0] = buffer[entry_offset];
			/*
			// load the next sector (if necessary) and set the offset
			// for the next byte in the buffer
			*/
			if (entry_offset == volume->no_of_bytes_per_serctor - 1)
			{
				/*
				// load the next sector into the buffer
				*/
				ret = volume->device->read_sector(volume->device->driver, entry_sector + 1, buffer);
				if (ret != STORAGE_SUCCESS)
				{
					FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
					FAT_UNLOCK_BUFFER();
					FAT_RELINQUISH_READ_ACCESS();
					return FAT_CANNOT_READ_MEDIA;
				}
				FAT_SET_LOADED_SECTOR(entry_sector + 1);
				/*
				// the 2nd byte is now the 1st byte in the buffer
				*/
				entry_offset = 0;
			}
			else
			{
				/*
				// the 2nd byte is still right after the 1st one on
				// the buffer
				*/
				entry_offset++;
			}
			/*
			// read the 2nd byte
			*/
			((unsigned char*) fat_entry)[1] = buffer[entry_offset];
			/*
			// Since a FAT12 entry is only 12 bits (1.5 bytes) we need to adjust the result.
			// For odd cluster numbers the FAT entry is stored in the upper 12 bits of the
			// 16 bits where it is stored, so we need to shift the value 4 bits to the right.
			// For even cluster numbers the FAT entry is stored in the lower 12 bits of the
			// 16 bits where it is stored, so we need to clear the upper 4 bits.
			*/
			if (cluster & 0x1)
			{
				*fat_entry >>= 4;
			}
			else
			{
				*fat_entry &= 0xFFF;
			}
			break;
		}
		case FAT_FS_TYPE_FAT16:
		{
			#if defined(BIG_ENDIAN)
			((unsigned char*) fat_entry)[INT32_BYTE0] = buffer[entry_offset + 0];
			((unsigned char*) fat_entry)[INT32_BYTE1] = buffer[entry_offset + 1];
			((unsigned char*) fat_entry)[INT32_BYTE2] = 0;
			((unsigned char*) fat_entry)[INT32_BYTE3] = 0;
			#else
			*fat_entry = (uint32_t) *((uint16_t*) &buffer[entry_offset]);
			#endif
			break;
		}
		case FAT_FS_TYPE_FAT32:
		{
			#if defined(BIG_ENDIAN)
			((unsigned char*) fat_entry)[INT32_BYTE0] = buffer[entry_offset + 0];
			((unsigned char*) fat_entry)[INT32_BYTE1] = buffer[entry_offset + 1];
			((unsigned char*) fat_entry)[INT32_BYTE2] = buffer[entry_offset + 2];
			((unsigned char*) fat_entry)[INT32_BYTE3] = buffer[entry_offset + 3] & 0x0F;
			#else
			*fat_entry = *((uint32_t*) &buffer[entry_offset]) & 0x0FFFFFFF;
			#endif
			break;
		}
	}
	/*
	// release the lock on the buffer
	*/
	FAT_UNLOCK_BUFFER();
	FAT_RELINQUISH_READ_ACCESS();
	/*
	// return the success code
	*/
	return FAT_SUCCESS;
}

/*
// updates the FAT entry for a given cluster
*/
#if !defined(FAT_READ_ONLY)
uint16_t fat_set_cluster_entry(FAT_VOLUME* volume, uint32_t cluster, FAT_ENTRY fat_entry ) 
{
	uint16_t ret;
	uint32_t fat_offset = 0;
	uint32_t entry_sector;
	uint32_t entry_offset;	

	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	unsigned char* buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	unsigned char* buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	#endif
	/*
	// get the offset of the entry in the FAT table for the requested cluster
	*/				
	switch (volume->fs_type)
	{
		case FAT_FS_TYPE_FAT12: fat_offset = cluster + (cluster >> 1); break;
		case FAT_FS_TYPE_FAT16: fat_offset = cluster * ((uint32_t) 2); break;
		case FAT_FS_TYPE_FAT32: fat_offset = cluster * ((uint32_t) 4); break;
	}
	/*
	// get the address of the sector that contains the FAT entry 
	// and the offset of the FAT entry within that sector
	*/
	entry_sector = volume->no_of_reserved_sectors + (fat_offset / volume->no_of_bytes_per_serctor);
	entry_offset = fat_offset % volume->no_of_bytes_per_serctor;
	/*
	// acquire lock on buffer
	*/
	FAT_ACQUIRE_WRITE_ACCESS();
	FAT_LOCK_BUFFER();
	/*
	// read sector into buffer
	*/
	if (!FAT_IS_LOADED_SECTOR(entry_sector))
	{
		ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
		if (ret != STORAGE_SUCCESS)
		{
			FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
			FAT_UNLOCK_BUFFER();
			FAT_RELINQUISH_WRITE_ACCESS();
			return FAT_CANNOT_READ_MEDIA;
		}
		FAT_SET_LOADED_SECTOR(entry_sector);
	}
	/*
	// set the FAT entry
	*/
	switch (volume->fs_type)
	{
		case FAT_FS_TYPE_FAT12:
		{
			/*
			// write the 1st byte
			*/
			if (cluster & 0x1)
			{
				fat_entry <<= 4;									/* odd entries occupy the upper 12 bits so we must shift */
				buffer[entry_offset] &= 0x0F;						/* clear entry bits on 1st byte */
				buffer[entry_offset] |= LO8((uint16_t) fat_entry);	/* set entry bits on 1st byte */
			}
			else
			{
				buffer[entry_offset] = LO8((uint16_t) fat_entry);	/* just copy the 1st byte */
			}
			/*
			// if the FAT entry spans a sector boundary flush the currently
			// loaded sector to the drive and load the next one.
			*/
			if (entry_offset == volume->no_of_bytes_per_serctor - 1)
			{
				/*
				// flush the updated sector to the drive
				*/
				fat_write_fat_sector(volume, entry_sector, buffer, &ret);
				if (ret != STORAGE_SUCCESS) 
				{
					FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
					FAT_UNLOCK_BUFFER();
					FAT_RELINQUISH_WRITE_ACCESS();
					return FAT_CANNOT_WRITE_MEDIA;
				}
				/*
				// increase the sector address
				*/
				entry_sector++;
				/*
				// load the next sector
				*/
				ret = volume->device->read_sector(volume->device->driver, entry_sector, buffer);
				if (ret != STORAGE_SUCCESS)
				{
					FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
					FAT_UNLOCK_BUFFER();
					FAT_RELINQUISH_WRITE_ACCESS();
					return FAT_CANNOT_READ_MEDIA;
				}
				FAT_SET_LOADED_SECTOR(entry_sector);
				/*
				// the next byte is now loacted at offset 0 on the buffer
				*/
				entry_offset = 0;
			}
			else
			{
				/*
				// the next byte is located next to the 1st one on the buffer
				*/
				entry_offset++;
			}
			/*
			// write the 2nd byte
			*/
			if (cluster & 0x1)
			{
				buffer[entry_offset] = HI8((uint16_t) fat_entry);	/* just copy the 1st byte */
			}
			else
			{
				buffer[entry_offset] &= 0xF0;						/* clear bits that 1st byte will be written to */
				buffer[entry_offset] |= HI8((uint16_t) fat_entry);	/* copy entry bits of 1st byte */
			}
			break;
		}
		case FAT_FS_TYPE_FAT16:
		{
			#if defined(BIG_ENDIAN)
			buffer[entry_offset + 0] = ((unsigned char*) &fat_entry)[INT32_BYTE0];
			buffer[entry_offset + 1] = ((unsigned char*) &fat_entry)[INT32_BYTE1];
			#else
			*((uint16_t*) &buffer[entry_offset]) = (uint16_t) fat_entry;
			#endif
			break;
		}
		case FAT_FS_TYPE_FAT32:
		{
			/*
			// since a FAT32 entry is actually 28 bits we need
			// to make sure that we don't modify the upper nibble.
			*/
			#if defined(BIG_ENDIAN)
			buffer[entry_offset + 0] = ((unsigned char*) &fat_entry)[INT32_BYTE0];
			buffer[entry_offset + 1] = ((unsigned char*) &fat_entry)[INT32_BYTE1];
			buffer[entry_offset + 2] = ((unsigned char*) &fat_entry)[INT32_BYTE2];
			buffer[entry_offset + 3] &= 0xF0;
			buffer[entry_offset + 3] |= ((unsigned char*) &fat_entry)[INT32_BYTE3] & 0x0F;
			#else
			*((uint32_t*) &buffer[entry_offset]) &= 0xF0000000;
			*((uint32_t*) &buffer[entry_offset]) |= fat_entry & 0x0FFFFFFF;
			#endif
			break;
		}
	}
	/*
	// write the updated sector to the
	// storage device
	*/
	fat_write_fat_sector(volume, entry_sector, buffer, &ret);
	if (ret != STORAGE_SUCCESS) 
	{
		FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
		FAT_UNLOCK_BUFFER();
		FAT_RELINQUISH_WRITE_ACCESS();
		return FAT_CANNOT_WRITE_MEDIA;
	}
	/*
	// return success code
	*/			
	FAT_UNLOCK_BUFFER();
	FAT_RELINQUISH_WRITE_ACCESS();
	return FAT_SUCCESS;
}
#endif

/*
// increase a cluster address by the amount of clusters indicated by count. This function will
// follow the FAT entry chain to fat the count-th cluster allocated to a file relative from the
// current_cluster cluster
*/
char fat_increase_cluster_address(
	FAT_VOLUME* volume, uint32_t cluster, uint16_t count, uint32_t* value ) 
{
	uint16_t ret;
	uint32_t fat_offset = 0;
	uint32_t entry_offset;
	uint32_t entry_sector;
	uint32_t current_sector;
	char is_odd_cluster = 0;
	char op_in_progress = 0;

	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	unsigned char* buffer = volume->sector_buffer;
	#elif defined(FAT_ALLOCATE_SHARED_BUFFER)
	unsigned char* buffer = fat_shared_buffer;
	#else
	ALIGN16 unsigned char buffer[MAX_SECTOR_LENGTH];
	#endif
	/*
	// if the count is zero we just return the same
	// cluster that we received
	*/
	if (!count)
	{
		*value = (uint32_t) cluster;
		return 1;
	}
	/*
	// get the offset of the cluster entry within the FAT table,
	// the sector of the FAT table that contains the entry and the offset
	// of the fat entry within the sector
	*/				
	FAT_CALCULATE_ENTRY_OFFSET(volume->fs_type, cluster, fat_offset);
	entry_sector = volume->no_of_reserved_sectors + (fat_offset / volume->no_of_bytes_per_serctor);
	entry_offset = fat_offset % volume->no_of_bytes_per_serctor;
	/*
	// acquire a lock on the buffer
	*/
	FAT_ACQUIRE_READ_ACCESS();
	FAT_LOCK_BUFFER();
	
	while (1)
	{
		current_sector = entry_sector;
		/*
		// read sector into hte buffer
		*/
		if (!FAT_IS_LOADED_SECTOR(current_sector))
		{
			ret = volume->device->read_sector(volume->device->driver, current_sector, buffer);
			if (ret != STORAGE_SUCCESS)
			{
				FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_READ_ACCESS();
				return 0;
			}
			FAT_SET_LOADED_SECTOR(current_sector);
		}
		/*
		// free all the fat entries on the current sector
		*/
		while (current_sector == entry_sector)
		{
			_ASSERT(cluster >= 2);
			/*
			// make sure we don't try to free an invalid cluster
			*/
			if (cluster < 2)
			{
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_READ_ACCESS();
				return FAT_INVALID_CLUSTER;
			}
			/*
			// read the cluster entry and mark it as free
			*/
			switch (volume->fs_type)
			{
				case FAT_FS_TYPE_FAT12:
				{
					if (!op_in_progress)
					{
						/*
						// remember whether this is an odd cluster or not
						*/
						is_odd_cluster = (cluster & 0x1);
						/*
						// set the cluster to zero to make sure that the upper bytes are cleared
						// since we're only updating the lower 16 bits. 
						*/
						cluster = 0;
						/*
						// read the 1st byte
						*/
						((unsigned char*) &cluster)[0] = buffer[entry_offset];
					}

					if (entry_offset == volume->no_of_bytes_per_serctor - 1)
					{
						/*
						// if the entry spans a sector boundary set op_in_progress to 1
						// so that we don't read the 1st byte again when we come back.
						// also increase the sector number and set the entry_offset to 0 since
						// the next byte will be on offset zero when the next sector is loaded
						*/
						entry_sector++;
						entry_offset = 0;
						op_in_progress = 1;
						/*
						// continue with the next iteration of the loop. We'll come right back
						// here with the next sector loaded
						*/
						continue;
					}
					else if (!op_in_progress)
					{
						/*
						// increase the offset to point to the next byte
						*/
						entry_offset++;
					}
					/*
					// read the 2nd byte
					*/
					((unsigned char*) &cluster)[1] = buffer[entry_offset];
					/*
					// Since a FAT12 entry is only 12 bits (1.5 bytes) we need to adjust the result.
					// For odd cluster numbers the FAT entry is stored in the upper 12 bits of the
					// 16 bits where it is stored, so we need to shift the value 4 bits to the right.
					// For even cluster numbers the FAT entry is stored in the lower 12 bits of the
					// 16 bits where it is stored, so we need to clear the upper 4 bits.
					*/
					if (is_odd_cluster)
					{
						cluster >>= 4;
					}
					else
					{
						cluster &= 0xFFF;
					}
					/*
					// clear op_in_progress flag.
					*/
					op_in_progress = 0;
					break;
				}
				case FAT_FS_TYPE_FAT16:
				{
					#if defined(BIG_ENDIAN)
					((unsigned char*) &cluster)[INT32_BYTE0] = buffer[entry_offset + 0];
					((unsigned char*) &cluster)[INT32_BYTE1] = buffer[entry_offset + 1];
					((unsigned char*) &cluster)[INT32_BYTE2] = 0;
					((unsigned char*) &cluster)[INT32_BYTE3] = 0;
					#else
					cluster = (uint32_t) *((uint16_t*) &buffer[entry_offset]);
					#endif
					break;
				}
				case FAT_FS_TYPE_FAT32:
				{
					#if defined(BIG_ENDIAN)
					((unsigned char*) &cluster)[INT32_BYTE0] = buffer[entry_offset + 0];
					((unsigned char*) &cluster)[INT32_BYTE1] = buffer[entry_offset + 1];
					((unsigned char*) &cluster)[INT32_BYTE2] = buffer[entry_offset + 2];
					((unsigned char*) &cluster)[INT32_BYTE3] = buffer[entry_offset + 3] & 0x0F;
					#else
					cluster = *((uint32_t*) &buffer[entry_offset]) & 0x0FFFFFFF;
					#endif
					break;
				}
			}
			/*
			// if the last cluster marks the end of the chian we return
			// false as we cannot increase the address by the # of clusters
			// requested by the caller
			*/
			if (fat_is_eof_entry(volume, cluster))
			{
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_READ_ACCESS();
				return 0;
			}
			/*
			// if we've followed the number of clusters requested by
			// the caller set the return value to the current cluster
			// and return true
			*/
			if (!--count)
			{
				FAT_UNLOCK_BUFFER();
				FAT_RELINQUISH_READ_ACCESS();
				*value = (uint32_t) cluster;
				return 1;
			}
			/*
			// calculate the location of the next cluster in the chain
			*/
			FAT_CALCULATE_ENTRY_OFFSET(volume->fs_type, cluster, fat_offset);
			entry_sector = volume->no_of_reserved_sectors + (fat_offset / volume->no_of_bytes_per_serctor);
			entry_offset = fat_offset % volume->no_of_bytes_per_serctor;
		}
	}
}

/*
// checks if a fat entry represents the
// last entry of a file
*/
char INLINE fat_is_eof_entry(FAT_VOLUME* volume, FAT_ENTRY fat) 
{
	switch (volume->fs_type) 
	{
		case FAT_FS_TYPE_FAT12 : return fat >= 0x0FF8;
		case FAT_FS_TYPE_FAT16 : return fat >= 0xFFF8;
		case FAT_FS_TYPE_FAT32 : return fat >= 0x0FFFFFF8;
	}
	_ASSERT(0);
	return 0;
}

/*
// initializes a directory cluster
*/ 
#if !defined(FAT_READ_ONLY)
static uint16_t INLINE fat_initialize_directory_cluster(
	FAT_VOLUME* volume, FAT_RAW_DIRECTORY_ENTRY* parent, uint32_t cluster, unsigned char* buffer) 
{
	uint16_t ret;
	uint16_t counter;
	uint32_t current_sector;
	FAT_RAW_DIRECTORY_ENTRY* entries;
	/*
	// if this is a big endian system or the compiler does not support
	// struct packing we cannot use FAT_RAW_DIRECTORY_ENTRY to write directly
	// to the buffer
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	FAT_RAW_DIRECTORY_ENTRY entry_storage;
	entries = &entry_storage;
	#else
	entries = (FAT_RAW_DIRECTORY_ENTRY*) buffer;
	#endif

	FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
	/*
	// initialize the 1st sector of the directory cluster with
	// the dot entry
	*/
	memset(buffer, 0, volume->no_of_bytes_per_serctor);
	entries->ENTRY.STD.name[0x0] = '.';
	entries->ENTRY.STD.name[0x1] = ' ';
	entries->ENTRY.STD.name[0x2] = ' ';
	entries->ENTRY.STD.name[0x3] = ' ';
	entries->ENTRY.STD.name[0x4] = ' ';
	entries->ENTRY.STD.name[0x5] = ' ';
	entries->ENTRY.STD.name[0x6] = ' ';
	entries->ENTRY.STD.name[0x7] = ' ';
	entries->ENTRY.STD.name[0x8] = ' ';
	entries->ENTRY.STD.name[0x9] = ' ';
	entries->ENTRY.STD.name[0xA] = ' ';
	entries->ENTRY.STD.attributes = FAT_ATTR_DIRECTORY;
	entries->ENTRY.STD.size = 0x0;
	entries->ENTRY.STD.reserved = 0;
	entries->ENTRY.STD.first_cluster_lo = LO16( cluster );
	entries->ENTRY.STD.first_cluster_hi = HI16( cluster );
	entries->ENTRY.STD.create_date = rtc_get_fat_date();
	entries->ENTRY.STD.create_time = rtc_get_fat_time();
	entries->ENTRY.STD.modify_date = entries->ENTRY.STD.create_date;
	entries->ENTRY.STD.modify_time = entries->ENTRY.STD.create_time;
	entries->ENTRY.STD.access_date = entries->ENTRY.STD.create_date;
	entries->ENTRY.STD.create_time_tenth = 0xc6;
	/*
	// write the entry to the buffer or move to the next entry
	// as required by target platform
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	fat_write_raw_directory_entry(entries, buffer);
	#else
	entries++;
	#endif
	/*
	// initialize the dot dot entry
	*/
	entries->ENTRY.STD.name[0x0] = '.';
	entries->ENTRY.STD.name[0x1] = '.';
	entries->ENTRY.STD.name[0x2] = ' ';
	entries->ENTRY.STD.name[0x3] = ' ';
	entries->ENTRY.STD.name[0x4] = ' ';
	entries->ENTRY.STD.name[0x5] = ' ';
	entries->ENTRY.STD.name[0x6] = ' ';
	entries->ENTRY.STD.name[0x7] = ' ';
	entries->ENTRY.STD.name[0x8] = ' ';
	entries->ENTRY.STD.name[0x9] = ' ';
	entries->ENTRY.STD.name[0xA] = ' ';
	entries->ENTRY.STD.attributes = FAT_ATTR_DIRECTORY;
	entries->ENTRY.STD.size = 0x0;
	entries->ENTRY.STD.reserved = 0;
	entries->ENTRY.STD.first_cluster_lo = parent->ENTRY.STD.first_cluster_lo;
	entries->ENTRY.STD.first_cluster_hi = parent->ENTRY.STD.first_cluster_hi;
	entries->ENTRY.STD.create_date= rtc_get_fat_date();
	entries->ENTRY.STD.create_time = rtc_get_fat_time();
	entries->ENTRY.STD.modify_date = entries->ENTRY.STD.create_date;
	entries->ENTRY.STD.modify_time = entries->ENTRY.STD.create_time;
	entries->ENTRY.STD.access_date = entries->ENTRY.STD.create_date;
	entries->ENTRY.STD.create_time_tenth = 0xc6;
	/*
	// when the parent is the root directory the dotdot entry always points
	// to cluster 0, even in FAT32 when the root directory is not actually on
	// cluster 0 so we need to check if the parent is the root directory and
	// in that case set the 1st cluster to 0
	*/
	if (volume->fs_type == FAT_FS_TYPE_FAT32)
	{
		uint32_t parent_cluster;
		((uint16_t*) &parent_cluster)[INT32_WORD0] = parent->ENTRY.STD.first_cluster_lo;
		((uint16_t*) &parent_cluster)[INT32_WORD1] = parent->ENTRY.STD.first_cluster_hi;

		if (volume->root_cluster == parent_cluster)
		{
			entries->ENTRY.STD.first_cluster_lo = 0;
			entries->ENTRY.STD.first_cluster_hi = 0;
		}
	}
	/*
	// copy entry to buffer
	*/
	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	fat_write_raw_directory_entry(entries, buffer + 0x20);
	#endif
	/*
	// write the 1st sector of the folder
	*/
	current_sector = FIRST_SECTOR_OF_CLUSTER(volume, cluster);
	ret = volume->device->write_sector(volume->device->driver, current_sector++, buffer);
	if (ret != STORAGE_SUCCESS)
		return FAT_CANNOT_WRITE_MEDIA;	
	/*
	// clear the . and .. entries from the buffer and
	// initialize the rest of the sectors of this cluster
	*/
	memset(buffer, 0, sizeof(FAT_RAW_DIRECTORY_ENTRY) * 2);
	counter = volume->no_of_sectors_per_cluster - 1;
	while (counter--)
	{
		ret = volume->device->write_sector(volume->device->driver, current_sector++, buffer);
		if (ret != STORAGE_SUCCESS)
			return FAT_CANNOT_WRITE_MEDIA;	
	}
	/*
	// return success code
	*/
	return FAT_SUCCESS;
}
#endif

/*
// sets all sectors in a cluster to zeroes
*/
#if !defined(FAT_READ_ONLY)
static uint16_t INLINE fat_zero_cluster(FAT_VOLUME* volume, uint32_t cluster, unsigned char* buffer) 
{

	uint16_t ret;
	uint16_t counter;
	uint32_t current_sector;

	FAT_SET_LOADED_SECTOR(0xFFFFFFFF);
	/*
	// set all the bytes in the buffer to zero
	*/
	memset(buffer, 0, volume->no_of_bytes_per_serctor);
	/*
	// calculate the address of the 1st sector
	// of the cluster
	*/
	current_sector = FIRST_SECTOR_OF_CLUSTER(volume, cluster);	
	counter = volume->no_of_sectors_per_cluster;
	/*
	// write the zeroed buffer to every sector in the cluster
	*/	
	while (counter--) 
	{
		ret = volume->device->write_sector(volume->device->driver, current_sector++, buffer);
		if (ret != FAT_SUCCESS)
			return FAT_CANNOT_WRITE_MEDIA;
	}
	/*
	// return success code
	*/
	return FAT_SUCCESS;
}	
#endif

/*
// writes a sector of the FAT table to the active table and (if option is enabled)
// to all other FAT tables
*/
#if !defined(FAT_READ_ONLY)
static INLINE void fat_write_fat_sector(FAT_VOLUME* volume, uint32_t sector_address, unsigned char* buffer, uint16_t* ret)
{
	/*
	// write the sector in the active FAT table
	*/
	*ret = volume->device->write_sector(volume->device->driver, sector_address, buffer);
	if (*ret != STORAGE_SUCCESS)
		return;
	/*
	// if we got more than one FAT table update the others as well
	*/
	#if defined(FAT_MAINTAIN_TWO_FAT_TABLES)
	if (volume->no_of_fat_tables > 1)
	{
		int i;
		for (i = 1; i < volume->no_of_fat_tables; i++)
		{
			*ret = volume->device->write_sector(volume->device->driver, sector_address + (volume->fat_size * i), buffer);
			if (*ret != STORAGE_SUCCESS)
				return;
		}
	}
	#endif
}
#endif
