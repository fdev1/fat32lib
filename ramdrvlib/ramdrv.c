/*
 * ramdrvlib - RAM Drive library for Fat32lib.
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

#include <compiler.h>
#include "..\fat32lib\storage_device.h"

typedef struct RAMDRIVE
{
	uint16_t id;
	unsigned char* buffer;
	uint16_t sector_size;
	uint32_t total_sectors;
}
RAMDRIVE;

uint16_t ramdrv_get_device_id(RAMDRIVE* device);
uint16_t ramdrv_get_sector_size(RAMDRIVE* device);
uint32_t ramdrv_get_total_sectors(RAMDRIVE* total_sectors);
void ramdrv_register_media_changed_callback(STORAGE_MEDIA_CHANGED_CALLBACK callback);
uint16_t ramdrv_read_sector(RAMDRIVE* ramdrive, uint32_t sector, unsigned char* buffer);
uint16_t ramdrv_write_sector(RAMDRIVE* ramdrive, uint32_t sector, unsigned char* buffer);

void ramdrv_init(RAMDRIVE* ramdrive, uint16_t total_sectors, uint16_t sector_size, unsigned char* buffer, STORAGE_DEVICE* device)
{
	ramdrive->buffer = buffer;
	ramdrive->sector_size = sector_size;
	ramdrive->total_sectors = total_sectors;

	device->Media = (void*) ramdrive;
	device->ReadSector = (STORAGE_DEVICE_READ) &ramdrv_read_sector;
	device->WriteSector = (STORAGE_DEVICE_WRITE) &ramdrv_write_sector;
	device->GetSectorSize = (STORAGE_DEVICE_GET_SECTOR_SIZE) &ramdrv_get_sector_size;
	device->GetTotalSectors = (STORAGE_DEVICE_GET_SECTOR_COUNT) &ramdrv_get_total_sectors;
	device->register_media_changed_callback = (STORAGE_REGISTER_MEDIA_CHANGED_CALLBACK) &ramdrv_register_media_changed_callback;
	device->get_device_id = (STORAGE_GET_DEVICE_ID) &ramdrv_get_device_id;
}

uint16_t ramdrv_get_device_id(RAMDRIVE* device)
{
	return device->id;
}

uint16_t ramdrv_get_sector_size(RAMDRIVE* device)
{
	return device->sector_size;
}

uint32_t ramdrv_get_total_sectors(RAMDRIVE* device)
{
	return device->total_sectors;
}

void ramdrv_register_media_changed_callback(STORAGE_MEDIA_CHANGED_CALLBACK callback)
{

}

uint16_t ramdrv_read_sector(RAMDRIVE* device, uint32_t sector, unsigned char* buffer)
{
	uint64_t offset;
	uint16_t i;
	offset = (uint64_t) sector * device->sector_size;

	for (i = 0; i < device->sector_size; i++)
	{
		device->buffer[offset + i] = buffer[i];
	}

	return STORAGE_SUCCESS;

}

uint16_t ramdrv_write_sector(RAMDRIVE* device, uint32_t sector, unsigned char* buffer)
{
	uint64_t offset;
	uint16_t i;
	offset = (uint64_t) sector * device->sector_size;

	for (i = 0; i < device->sector_size; i++)
	{
		buffer[i] = device->buffer[offset + i];
	}

	return STORAGE_SUCCESS;

}
