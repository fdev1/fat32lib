<?xml version="1.0"?>
<doc>
<members>
<member name="D:FAT_GET_SYSTEM_TIME" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="248">
<summary>Function pointer to get the system time.</summary>
</member>
<member name="T:FAT_VOLUME" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="251">
<summary>Volume handle structure.</summary>
</member>
<member name="T:FAT_DIRECTORY_ENTRY" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="332">
<summary>
Stores information about directory entries.
</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.name" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="337">
<summary>The name of the file.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.attributes" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="339">
<summary>The list of file attributes ORed together.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.create_time" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="341">
<summary>The creation timestamp of the file.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.modify_time" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="343">
<summary>The modification timestamp of the file.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.access_time" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="345">
<summary>The access timestamp of the file.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.size" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="347">
<summary>The size of the file.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.sector_addr" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="349">
<summary>Reserved for internal use.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.sector_offset" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="351">
<summary>Reserved for internal use.</summary>
</member>
<member name="F:FAT_DIRECTORY_ENTRY.raw" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="353">
<summary>Reserved for internal use.</summary>
</member>
<member name="D:FAT_ASYNC_CALLBACK" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="385">
<summary>Callback function for asynchronous IO.</summary>
</member>
<member name="T:FAT_FILE" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="410">
<summary>File handle structure</summary>
</member>
<member name="T:FILESYSTEM_QUERY" decl="false" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="433">
<summary>Holds the state of a directory query.</summary>
</member>
<!-- Discarding badly formed XML document comment for member 'M:fat_init'. -->
<member name="M:fat_mount_volume(FAT_VOLUME*,_STORAGE_DEVICE*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="464">
<sumary>
Mounts a FAT volume.
</sumary>
<param name="volume">A pointer to a volume handle structure (FAT_VOLUME).</param>
<param name="device">A pointer to the storage device driver STORAGE_DEVICE structure.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<!-- Discarding badly formed XML document comment for member 'M:fat_get_sector_size(FAT_VOLUME*)'. -->
<member name="M:fat_find_first_entry(FAT_VOLUME*,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*,System.Byte,FAT_DIRECTORY_ENTRY**,FILESYSTEM_QUERY*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="498">
<sumary>
Finds the first entry in a directory.
</sumary>
<param name="volume">A pointer to the volume handle (FAT_VOLUME).</param>
<param name="parent_path">The path of the directory to query.</param>
<param name="attributes">An ORed list of file attributes to filter the query.</param>
<param name="dir_entry">
A pointer-to-pointer to a FAT_DIRECTORY_ENTRY structure. 
When this function returns the pointer will be set to to point to the directory entry.
</param>
<param name="query">A pointer to a FAT_FILESYSTEM_QUERY that will be initialized as the query handle.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_find_next_entry(FAT_VOLUME*,FAT_DIRECTORY_ENTRY**,FILESYSTEM_QUERY*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="519">
<sumary>
Finds the next entry in a directory.
</sumary>
<param name="volume">A pointer to the volume handle (FAT_VOLUME).</param>
<param name="dir_entry">
A pointer-to-pointer to a FAT_DIRECTORY_ENTRY structure.
When this function returns the pointer will be set to point the the directory entry.
</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_create_directory(FAT_VOLUME*,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="535">
<sumary>
Creates a new directory on the volume.
</sumary>
<param name="volume">A pointer to the volume handle (FAT_VOLUME).</param>
<param name="filename">The full path to the new directory.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_file_delete(FAT_VOLUME*,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="547">
<sumary>
Deletes a file.
</sumary>
<param name="volume">A pointer to the volume handle (FAT_VOLUME structure).</param>
<param name="filename">The full path and filename of the file to delete.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_file_rename(FAT_VOLUME*,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="559">
<sumary>
Renames a file.
</sumary>
<param name="volume">A pointer to the volume handle (FAT_VOLUME structure).</param>
<param name="original_filename">The full path and original filename of the file to be renamed.</param>
<param name="new_filename">The full path and new filename for the file.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_file_open(FAT_VOLUME*,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte*,System.Byte,FAT_FILE*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="573">
<sumary>
Opens or create a file.
</sumary>
<param name="volume">A pointer to the volume handle (FAT_VOLUME structure).</param>
<param name="filename">The full path and filename of the file to open.</param>
<param name="access_flags">An ORed list of one or more of the access flags defined in fat.h</param>
<param name="file">A pointer to a file handle FAT_FILE structure.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_file_set_buffer(FAT_FILE*,System.Byte*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="589">
<summary>
Sets an external buffer for this file handle.
</summary>
</member>
<member name="M:fat_file_alloc(FAT_FILE*,System.UInt32)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="598">
<sumary>
Allocates disk space to an open file.
</sumary>
<param name="file">A pointer to a file handle FAT_FILE structure.</param>
<param name="bytes">The amount of disk space (in bytes) to be allocated.</param>
<returns>One of the return codes defined in fat.h.</returns>
<remarks>
When writting large files in small chunks calling this function to pre-allocate 
drive space significantly improves write speed. Any space that is not used will be freed
when the file is closed.

This function will allocate enough disk space to write the requested amount of
bytes after the current poisition of the file. So if you request 5K bytes and there's 
already 2K bytes allocated after the cursor position it will only allocate 3K bytes.
All disk allocations are done in multiples of the cluster size.
</remarks>
</member>
<member name="M:fat_file_seek(FAT_FILE*,System.UInt32,System.SByte!System.Runtime.CompilerServices.IsSignUnspecifiedByte)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="620">
<summary>
Moves the file cursor to a new position within the file.
</summary>
<param name="file">A pointer to the file handle FAT_FILE structure.</param>
<param name="offset">The offset of the new cursor position relative to the position specified by the mode param.</param>
<param name="mode">One of the supported seek modes: FAT_SEEK_START, FAT_SEEK_CURRENT, or FAT_SEEK_END.</param>
<returns>One of the return codes defined in fat.h.</returns>
<remarks>
If FAT_SEEK_END is specified the offset must be 0 or the function will fail.
</remarks>
</member>
<member name="M:fat_file_write(FAT_FILE*,System.Byte*,System.UInt32)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="637">
<summary>
Writes the specified number of bytes to the current position on an opened file.
</summary>
<param name="file">A pointer to a file handle FAT_FILE structure.</param>
<param name="buffer">A buffer containing the bytes to be written.</param>
<param name="length">The amount of bytes to write.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_file_write_async(FAT_FILE*,System.Byte*,System.UInt32,System.UInt16*,=FUNC:System.Void(System.Void*,System.UInt16*),System.Void*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="651">
<summary>
Writes the specified number of bytes to the current position on an opened file asynchronously.
</summary>
<param name="file">A pointer to a file handle FAT_FILE structure.</param>
<param name="buffer">A buffer containing the bytes to be written.</param>
<param name="length">The amount of bytes to write.</param>
<param name="result">A pointer to a 16-bit unsigned integer where the result of the operation will be saved.</param>
<param name="callback">A callback function that will be called when the operation completes.</param>
<returns>One of the return codes defined in fat.h.</returns>
<remarks>
If the operation is initialized successfully this function will return FAT_SUCCESS, however the operation
may still fail and the actual return code is stored on the 16 bit integer pointed to by the result parameter
when the operation completes.
</remarks>
</member>
<member name="M:fat_file_read(FAT_FILE*,System.Byte*,System.UInt32,System.UInt32*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="675">
<summary>
Reads the specified number of bytes from the current position on an opened file.
</summary>
<param name="file">A pointer to a file handle FAT_FILE structure.</param>
<param name="buffer">A buffer where the bytes will be copied to.</param>
<param name="length">The amount of bytes to read.</param>
<param name="bytes_read">A pointer to a 32 bit integer where the amount of bytes read will be written to.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
<member name="M:fat_file_read_async(FAT_FILE*,System.Byte*,System.UInt32,System.UInt32*,System.UInt16*,=FUNC:System.Void(System.Void*,System.UInt16*),System.Void*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="691">
<summary>
Reads the specified number of bytes from the current position on an opened file asynchronously.
</summary>
<param name="file">A pointer to a file handle FAT_FILE structure.</param>
<param name="buffer">A buffer where the bytes will be copied to.</param>
<param name="length">The amount of bytes to read.</param>
<param name="bytes_read">A pointer to a 32 bit integer where the amount of bytes read will be written to.</param>
<param name="result">A pointer to a 16-bit unsigned integer where the result of the operation will be saved.</param>
<param name="callback">A callback function that will be called when the operation completes.</param>
<returns>One of the return codes defined in fat.h.</returns>
<remarks>
If the operation is initialized successfully this function will return FAT_SUCCESS, however the operation
may still fail and the actual return code is stored on the 16 bit integer pointed to by the result parameter
when the operation completes.
</remarks>
</member>
<member name="M:fat_file_flush(FAT_FILE*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="717">
<summary>
Flushes file buffers and updates directory entry.
</summary>
<param name="file">A pointer to the file handle (FAT_FILE) structure.</param>
</member>
<member name="M:fat_file_close(FAT_FILE*)" decl="true" source="c:\users\fernan\documents\fat32lib\fat32lib\fat.h" line="726">
<summary>
Closes an open file.
</summary>
<param name="handle">A pointer to the file handle FAT_FILE structure.</param>
<returns>One of the return codes defined in fat.h.</returns>
</member>
</members>
</doc>
