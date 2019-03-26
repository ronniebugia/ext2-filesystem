#include "ext2.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fsuid.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>

#define EXT2_OFFSET_SUPERBLOCK 1024
#define EXT2_INVALID_BLOCK_NUMBER ((uint32_t) -1)

/* open_volume_file: Opens the specified file and reads the initial
   EXT2 data contained in the file, including the boot sector, file
   allocation table and root directory.
   
   Parameters:
     filename: Name of the file containing the volume data.
   Returns:
     A pointer to a newly allocated volume_t data structure with all
     fields initialized according to the data in the volume file
     (including superblock and group descriptor table), or NULL if the
     file is invalid or data is missing.
 */
volume_t *open_volume_file(const char *filename) {
  
  /* TO BE COMPLETED BY THE STUDENT */

  FILE *fileptr;
  
  volume_t *volume;
  volume = malloc(sizeof(volume_t));
  
  fileptr = fopen(filename, "rb"); // opened in binary mode
  fseek(fileptr, EXT2_OFFSET_SUPERBLOCK, SEEK_SET); // offset to superblock (1024)
  
  // PARSE SUPERBLOCK
  superblock_t *superblock;
  superblock = malloc(sizeof(superblock_t));
  superblock->s_inodes_count = make_uint32(read_bytes(fileptr, 4)); // 0
  superblock->s_blocks_count = make_uint32(read_bytes(fileptr, 4)); // 4
  superblock->s_r_blocks_count = make_uint32(read_bytes(fileptr, 4)); // 8
  superblock->s_free_blocks_count = make_uint32(read_bytes(fileptr, 4)); // 12
  superblock->s_free_inodes_count = make_uint32(read_bytes(fileptr, 4)); // 16
  superblock->s_first_data_block = make_uint32(read_bytes(fileptr, 4)); // 20
  superblock->s_log_block_size = make_uint32(read_bytes(fileptr, 4)); // 24
  superblock->s_log_frag_size = make_uint32(read_bytes(fileptr, 4)); // 28
  superblock->s_blocks_per_group = make_uint32(read_bytes(fileptr, 4)); // 32
  superblock->s_frags_per_group = make_uint32(read_bytes(fileptr, 4)); // 36
  superblock->s_inodes_per_group = make_uint32(read_bytes(fileptr, 4)); // 40
  superblock->s_mtime = make_uint32(read_bytes(fileptr, 4)); // 44
  superblock->s_wtime = make_uint32(read_bytes(fileptr, 4)); // 48
  superblock->s_mnt_count = make_uint16(read_bytes(fileptr, 2)); // 52
  superblock->s_max_mnt_count = make_uint16(read_bytes(fileptr, 2)); // 43
  superblock->s_magic = make_uint16(read_bytes(fileptr, 2)); // 56
  superblock->s_state = make_uint16(read_bytes(fileptr, 2)); // 58
  superblock->s_errors = make_uint16(read_bytes(fileptr, 2)); // 60
  superblock->s_minor_rev_level = make_uint16(read_bytes(fileptr, 2)); // 62
  superblock->s_lastcheck = make_uint32(read_bytes(fileptr, 4)); // 64
  superblock->s_checkinterval = make_uint32(read_bytes(fileptr, 4)); // 68
  superblock->s_creator_os = make_uint32(read_bytes(fileptr, 4)); // 72
  superblock->s_rev_level = make_uint32(read_bytes(fileptr, 4)); // 76
  superblock->s_def_resuid = make_uint16(read_bytes(fileptr, 2)); // 80
  superblock->s_def_resgid = make_uint16(read_bytes(fileptr, 2)); // 82
  superblock->s_first_ino = make_uint32(read_bytes(fileptr, 4)); // 84
  superblock->s_inode_size = make_uint16(read_bytes(fileptr, 2)); // 88
  superblock->s_block_group_nr = make_uint16(read_bytes(fileptr, 2)); // 90
  superblock->s_feature_compat = make_uint32(read_bytes(fileptr, 4)); // 92
  superblock->s_feature_incompat = make_uint32(read_bytes(fileptr, 4)); // 96
  superblock->s_feature_ro_compat = make_uint32(read_bytes(fileptr, 4)); // 100
  // TODO: parsing UUID, volume names, last mounted, bitmap
  
  volume->super = *superblock;
  
  fclose(fileptr);
  
  return volume;
}

/* HELPER - read_bytes: Reads a specified number of bytes from the file pointer.

   Parameters:
     fileptr: pointer to the file currently being read
     noBytes: number of bytes to read
*/
unsigned char* read_bytes(FILE* fileptr, unsigned int noBytes) {
  unsigned char *bytes;
  bytes = malloc (sizeof(unsigned char) * noBytes);
  fread(bytes, 1, noBytes, fileptr);
  /* for (int i = 0; i < noBytes; i++)
    printf("%02x\n", bytes[i]); */
  return bytes;
}

/* HELPER - make_uint32: Reads 4 bytes in Little Endian and converts to a 32-bit unsigned int.

   Parameters:
      bytes: array of 4 unsigned chars to be converted
*/
uint32_t make_uint32(unsigned char* bytes) {
  uint32_t num = (uint32_t) bytes[3] << 24 |
    (uint32_t) bytes[2] << 16 |
    (uint32_t) bytes[1] << 8 |
    (uint32_t) bytes[0];
  return num;
}

/* HELPER - make_uint16: Reads 2 bytes in Little Endian and converts to a 16-bit unsigned int.

   Parameters:
      bytes: array of 2 unsigned chars to be converted
*/
uint16_t make_uint16(unsigned char* bytes) {
  uint16_t num = (uint16_t) bytes[1] << 8 |
    (uint16_t) bytes[0];
  return num;
}

/* HELPER - make_uint8: Reads 1 byte in Little Endian and converts to a 8-bit unsigned int.

   Parameters:
      bytes: array of 1 unsigned char to be converted
*/
uint8_t make_uint8(unsigned char* bytes) {
  uint8_t num = (uint8_t) bytes[0];
  return num;
}

/* close_volume_file: Frees and closes all resources used by a EXT2 volume.
   
   Parameters:
     volume: pointer to volume to be freed.
 */
void close_volume_file(volume_t *volume) {

  /* TO BE COMPLETED BY THE STUDENT */
}

/* read_block: Reads data from one or more blocks. Saves the resulting
   data in buffer 'buffer'. This function also supports sparse data,
   where a block number equal to 0 sets the value of the corresponding
   buffer to all zeros without reading a block from the volume.
   
   Parameters:
     volume: pointer to volume.
     block_no: Block number where start of data is located.
     offset: Offset from beginning of the block to start reading
             from. May be larger than a block size.
     size: Number of bytes to read. May be larger than a block size.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_block(volume_t *volume, uint32_t block_no, uint32_t offset, uint32_t size, void *buffer) {

  /* TO BE COMPLETED BY THE STUDENT */
  return -1;
}

/* read_inode: Fills an inode data structure with the data from one
   inode in disk. Determines the block group number and index within
   the group from the inode number, then reads the inode from the
   inode table in the corresponding group. Saves the inode data in
   buffer 'buffer'.
   
   Parameters:
     volume: pointer to volume.
     inode_no: Number of the inode to read from disk.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns a positive value. In case of error,
     returns -1.
 */
ssize_t read_inode(volume_t *volume, uint32_t inode_no, inode_t *buffer) {
  
  /* TO BE COMPLETED BY THE STUDENT */
  return -1;
}

/* read_ind_block_entry: Reads one entry from an indirect
   block. Returns the block number found in the corresponding entry.
   
   Parameters:
     volume: pointer to volume.
     ind_block_no: Block number for indirect block.
     index: Index of the entry to read from indirect block.

   Returns:
     In case of success, returns the block number found at the
     corresponding entry. In case of error, returns
     EXT2_INVALID_BLOCK_NUMBER.
 */
static uint32_t read_ind_block_entry(volume_t *volume, uint32_t ind_block_no,
				     uint32_t index) {
  
  /* TO BE COMPLETED BY THE STUDENT */
  return 0;
}

/* read_inode_block_no: Returns the block number containing the data
   associated to a particular index. For indices 0-11, returns the
   direct block number; for larger indices, returns the block number
   at the corresponding indirect block.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure where data is to be sourced.
     index: Index to the block number to be searched.

   Returns:
     In case of success, returns the block number to be used for the
     corresponding entry. This block number may be 0 (zero) in case of
     sparse files. In case of error, returns
     EXT2_INVALID_BLOCK_NUMBER.
 */
static uint32_t get_inode_block_no(volume_t *volume, inode_t *inode, uint32_t block_idx) {
  
  /* TO BE COMPLETED BY THE STUDENT */
  return EXT2_INVALID_BLOCK_NUMBER;
}

/* read_file_block: Returns the content of a specific file, limited to
   a single block.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the file.
     offset: Offset, in bytes from the start of the file, of the data
             to be read.
     max_size: Maximum number of bytes to read from the block.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_file_block(volume_t *volume, inode_t *inode, uint32_t offset, uint32_t max_size, void *buffer) {
    
  /* TO BE COMPLETED BY THE STUDENT */
  return -1;
}

/* read_file_content: Returns the content of a specific file, limited
   to the size of the file only. May need to read more than one block,
   with data not necessarily stored in contiguous blocks.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the file.
     offset: Offset, in bytes from the start of the file, of the data
             to be read.
     max_size: Maximum number of bytes to read from the file.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_file_content(volume_t *volume, inode_t *inode, uint32_t offset, uint32_t max_size, void *buffer) {

  uint32_t read_so_far = 0;

  if (offset + max_size > inode_file_size(volume, inode))
    max_size = inode_file_size(volume, inode) - offset;
  
  while (read_so_far < max_size) {
    int rv = read_file_block(volume, inode, offset + read_so_far,
			     max_size - read_so_far, buffer + read_so_far);
    if (rv <= 0) return rv;
    read_so_far += rv;
  }
  return read_so_far;
}

/* follow_directory_entries: Reads all entries in a directory, calling
   function 'f' for each entry in the directory. Stops when the
   function returns a non-zero value, or when all entries have been
   traversed.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the directory.
     context: This pointer is passed as an argument to function 'f'
              unmodified.
     buffer: If function 'f' returns non-zero for any file, and this
             pointer is set to a non-NULL value, this buffer is set to
             the directory entry for which the function returned a
             non-zero value. If the pointer is NULL, nothing is
             saved. If none of the existing entries returns non-zero
             for 'f', the value of this buffer is unspecified.
     f: Function to be called for each directory entry. Receives three
        arguments: the file name as a NULL-terminated string, the
        inode number, and the context argument above.

   Returns:
     If the function 'f' returns non-zero for any directory entry,
     returns the inode number for the corresponding entry. If the
     function returns zero for all entries, or the inode is not a
     directory, or there is an error reading the directory data,
     returns 0 (zero).
 */
uint32_t follow_directory_entries(volume_t *volume, inode_t *inode, void *context,
				  dir_entry_t *buffer,
				  int (*f)(const char *name, uint32_t inode_no, void *context)) {

  /* TO BE COMPLETED BY THE STUDENT */
  return 0;
}

/* Simple comparing function to be used as argument in find_file_in_directory function */
static int compare_file_name(const char *name, uint32_t inode_no, void *context) {
  return !strcmp(name, (char *) context);
}

/* find_file_in_directory: Searches for a file in a directory.
   
   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the directory.
     name: NULL-terminated string for the name of the file. The file
           name must match this name exactly, including case.
     buffer: If the file is found, and this pointer is set to a
             non-NULL value, this buffer is set to the directory entry
             of the file. If the pointer is NULL, nothing is saved. If
             the file is not found, the value of this buffer is
             unspecified.

   Returns:
     If the file exists in the directory, returns the inode number
     associated to the file. If the file does not exist, or the inode
     is not a directory, or there is an error reading the directory
     data, returns 0 (zero).
 */
uint32_t find_file_in_directory(volume_t *volume, inode_t *inode, const char *name, dir_entry_t *buffer) {
  
  return follow_directory_entries(volume, inode, (char *) name, buffer, compare_file_name);
}

/* find_file_from_path: Searches for a file based on its full path.
   
   Parameters:
     volume: Pointer to volume.
     path: NULL-terminated string for the full absolute path of the
           file. Must start with '/' character. Path components
           (subdirectories) must be delimited by '/'. The root
           directory can be obtained with the string "/".
     dest_inode: If the file is found, and this pointer is set to a
                 non-NULL value, this buffer is set to the inode of
                 the file. If the pointer is NULL, nothing is
                 saved. If the file is not found, the value of this
                 buffer is unspecified.

   Returns:
     If the file exists, returns the inode number associated to the
     file. If the file does not exist, or there is an error reading
     any directory or inode in the path, returns 0 (zero).
 */
uint32_t find_file_from_path(volume_t *volume, const char *path, inode_t *dest_inode) {

  /* TO BE COMPLETED BY THE STUDENT */
  return 0;
}
