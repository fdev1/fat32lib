<?xml version="1.0"?>
<doc>
<members>
<member name="M:fat_format_volume(System.Byte,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*!System.Runtime.CompilerServices.IsConst,System.UInt32,_STORAGE_DEVICE*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="399">
<sumary>
Formats a FAT volume. The sector size of the drive must be 512 bytes.
</sumary>

<param name="fs_type">
A constant identifying the file system type.
Possible values:
FAT_FS_TYPE_FAT12
FAT_FS_TYPE_FAT16
FAT_FS_TYPE_FAT32
</param>
<param name="volume_label">The volume label for the drive. Must be 12 characters or less.</param>
<param name="no_of_sectors_per_cluster">The number of sectors per cluster. Must be a power of 2 between 2 and 64.</param>
<param name="device">A pointer to a STORAGE_DEVICE structure that represents the storage device driver.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_format_volume(System.Byte,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*!System.Runtime.CompilerServices.IsConst,System.UInt32,_STORAGE_DEVICE*)" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat_utilities.cpp" line="158">
<sumary>
Formats a FAT volume. The sector size of the drive must be 512 bytes.
</sumary>

<param name="fs_type">
A constant identifying the file system type.
Possible values:
FAT_FS_TYPE_FAT12
FAT_FS_TYPE_FAT16
FAT_FS_TYPE_FAT32
</param>
<param name="volume_label">The volume label for the drive. Must be 12 characters or less.</param>
<param name="no_of_sectors_per_cluster">The number of sectors per cluster. Must be a power of 2 between 2 and 64.</param>
<param name="device">A pointer to a STORAGE_DEVICE structure that represents the storage device driver.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
</members>
</doc>
