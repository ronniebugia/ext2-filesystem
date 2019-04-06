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
  
  int filedesc; // file descriptor
  filedesc = open(filename, O_RDONLY);

  if (filedesc < 0)
    return NULL;
  
  lseek(filedesc, EXT2_OFFSET_SUPERBLOCK, SEEK_SET); // offset to superblock

  volume_t *volume;
  volume = malloc(sizeof(volume_t));
  // SET FILE DESCRIPTOR
  volume->fd = filedesc;
  
  // PARSE SUPERBLOCK
  superblock_t *superblock;
  superblock = malloc(sizeof(superblock_t));
  int bytes_read = read(filedesc, superblock, sizeof(superblock_t));

  if (bytes_read < 0) {
    free(volume);
    free(superblock);
    return NULL;
  }
  
  volume->super = *superblock;
  free(superblock);
  
  // BLOCK SIZE, VOLUME SIZE, NUM_GROUPS
  volume->block_size = 1024 << volume->super.s_log_block_size;
  volume->volume_size = volume->super.s_blocks_count * volume->block_size;
  volume->num_groups = (volume->super.s_blocks_count + volume->super.s_blocks_per_group - 1) / volume->super.s_blocks_per_group;
  printf("num groups: %d\n", volume->num_groups);
  
  // SEEK TO GROUP DESCRIPTOR TABLE
  if (volume->block_size >= 2048)
    lseek(filedesc, volume->block_size, SEEK_SET);
  else
    lseek(filedesc, volume->block_size * 2, SEEK_SET);
  
  // PARSE GROUP DESCRIPTOR TABLE
  group_desc_t *groups;
  groups = malloc(sizeof(group_desc_t) * volume->num_groups);
  read(filedesc, groups, sizeof(group_desc_t) * volume->num_groups);
  volume->groups = groups;

  return volume;
} 

/* close_volume_file: Frees and closes all resources used by a EXT2 volume.
   
   Parameters:
     volume: pointer to volume to be freed.
 */
void close_volume_file(volume_t *volume) {
  close(volume->fd);
  free(volume);
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

  if (block_no == 0) {
    memset(buffer, 0, size);
    return 0;
  }
  
  lseek(volume->fd, block_no * volume->block_size + offset, SEEK_SET);
  return read(volume->fd, buffer, size);
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

  if (inode_no == 0)
    return -1;
  
  /// change to return read_block ??
  // determine block group number and index within the group from the inode number
  //printf("volume last mounted: %s\n", volume->super.s_last_mounted);
  //printf("inode num: %d\n", inode_no);
  int block_group = (inode_no - 1) / volume->super.s_inodes_per_group;
  uint32_t inode_table = volume->groups[block_group].bg_inode_table;
  //printf("block_group: %d, inode_table %d\n", block_group, inode_table);
  
  int local_inode_index = (inode_no - 1) % volume->super.s_inodes_per_group;
  uint32_t offset = local_inode_index * volume->super.s_inode_size;
  uint16_t size = volume->super.s_inode_size;
  
  // read inode from inode table into corresponding group
  return read_block(volume, inode_table, offset, size, buffer);
  
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

  int block_no;
  int retval = read_block(volume, ind_block_no, index * 4, 4, &block_no); 
  if (retval > 0)
    return block_no;
  else
    return EXT2_INVALID_BLOCK_NUMBER;
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
static uint32_t get_inode_block_no(volume_t *volume, inode_t *inode, uint64_t block_idx) {
  
  // determine block group number within the group from the inode number
  // int block_group = (inode_no - 1) / volume->super.s_inodes_per_group;

  int block_size = volume->block_size;
  int num_entries = block_size / 4; // each entry is 4 bytes

  int num_single_entries = num_entries;
  int num_double_entries = num_entries * num_entries;
  int num_triple_entries = num_entries * num_entries * num_entries;
  
  int first_indirect_block = 12;
  int first_double_indirect_block = first_indirect_block + num_single_entries;
  int first_triple_indirect_block = first_double_indirect_block + num_double_entries;
  int max_valid_index = first_triple_indirect_block + num_triple_entries;

  
  if (block_idx < 12) { // direct blocks
    return inode->i_block[block_idx];
  }

  else if (block_idx >= first_indirect_block && block_idx < first_double_indirect_block) { // single indirect block
    return read_ind_block_entry(volume, inode->i_block_1ind, block_idx - first_indirect_block);
  }

  else if (block_idx >= first_double_indirect_block && block_idx < first_triple_indirect_block) { // double indirect block
    int tmp;
    int local_index = (block_idx - first_double_indirect_block) / num_double_entries;
    tmp = read_ind_block_entry(volume, inode->i_block_2ind, local_index);
    return read_ind_block_entry(volume, tmp, block_idx - first_double_indirect_block);
  }

  else if (block_idx >= first_triple_indirect_block && block_idx < max_valid_index) { // triple indirect block
    int tmp;
    int local_index = ((block_idx - first_triple_indirect_block) / num_double_entries) / num_triple_entries;
    tmp = read_ind_block_entry(volume, inode->i_block_3ind, local_index);
    local_index = (block_idx - first_triple_indirect_block) / num_double_entries;
    tmp = read_ind_block_entry(volume, tmp, local_index);
    return read_ind_block_entry(volume, tmp, block_idx - first_triple_indirect_block);
  }

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
ssize_t read_file_block(volume_t *volume, inode_t *inode, uint64_t offset, uint64_t max_size, void *buffer) {
  // because offset = block_size * block
  uint32_t block_no = get_inode_block_no(volume, inode, offset / volume->block_size);
  uint32_t local_offset = offset % volume->block_size;
  // printf("READ INODE: %d", buffer->i_mode);
  return read_block(volume, block_no, local_offset, max_size, buffer);
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
ssize_t read_file_content(volume_t *volume, inode_t *inode, uint64_t offset, uint64_t max_size, void *buffer) {
  
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

  //printf("follow_directory_entries");
  if ((inode->i_mode >> 12) != 4)
    return 0;

  // somehow read entries in the directory
  // for entry, call function f with arg context
  int next_offset = 0;
  int retval = 0;
  int inode_size = inode_file_size(volume, inode);
  dir_entry_t *dir_entry;
  dir_entry = malloc(sizeof(dir_entry_t));

  while (next_offset < inode_size) {
    // int block_no = get_inode_block_no(volume, inode, i);
    ssize_t bytes_read = read_file_content(volume, inode, next_offset, sizeof(dir_entry_t), dir_entry);
    if (bytes_read == -1)
      break;
    if (dir_entry->de_inode_no == 0) // means null
      break;
    //printf("filename read: %s\n", dir_entry->de_name);

    char* tmp_de_name = malloc(sizeof(char) * (dir_entry->de_name_len + 1));
    memcpy(tmp_de_name, dir_entry->de_name, dir_entry->de_name_len);
    tmp_de_name[dir_entry->de_name_len] = '\0';
    int result = f(tmp_de_name, dir_entry->de_inode_no, context);
    if (result != 0) {
      dir_entry->de_name[dir_entry->de_name_len] = '\0';
      dir_entry->de_name_len = dir_entry->de_name_len + 1;
      retval = dir_entry->de_inode_no;
      if (buffer != NULL) // copy into the buffer
	memcpy(buffer, dir_entry, sizeof(dir_entry_t)); 
      break;
    }
    next_offset += dir_entry->de_rec_len;
  }

  free(dir_entry);
  return retval;
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
  //printf("finding file: %s\n", name);
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
  inode_t *dir_inode;
  dir_inode = malloc(sizeof(inode_t));

  char *path_str = malloc(sizeof(char) * (strlen(path) + 1)); // extra for null 
  strcpy(path_str, path);

  char* token = strtok(path_str, "/");
  
  int curr_inode = 2;
  
  read_inode(volume, curr_inode, dir_inode);

  if (token == NULL) { // looking for root directory
    //printf("root found\n");
    memcpy(dest_inode, dir_inode, volume->super.s_inode_size);
    free(dir_inode);
    free(path_str);
    return 2;
  }

  dir_entry_t *dir_entry;
  dir_entry = malloc(sizeof(dir_entry_t));

  while (token != NULL && curr_inode > 0) {
    //printf("/%s\n", token);
    curr_inode = find_file_in_directory(volume, dir_inode, token, dir_entry); 
    if (curr_inode == 0) {
      free(dir_inode);
      free(dir_entry);
      free(path_str);
      return 0; // error
    }
    read_inode(volume, curr_inode, dir_inode);
    token = strtok(0, "/"); 
  }

  free(dir_inode);
  free(dir_entry);
  free(path_str);
    
  if (curr_inode > 0) {
    read_inode(volume, curr_inode, dest_inode);
    return curr_inode;
  }

  return 0;
}
