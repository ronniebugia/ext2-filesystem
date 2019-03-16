#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"

static int print_dir_entry(const char *name, uint32_t inode_no, void *context) {
  printf("    %#8" PRIx32 ": %s\n", inode_no, name);
  return 0;
}

static int print_dir_entry_recursive(const char *name, uint32_t inode_no, void *context) {
  
  static int num_spaces = 0;
  
  volume_t *volume = context;
  inode_t inode;
  
  if (!strcmp(".", name) || !strcmp("..", name)) return 0;
  
  read_inode(volume, inode_no, &inode);
  printf("%*s/%s (blocks:", num_spaces, "", name);
  for (int i = 0; i < 12; i++) {
    if (inode.i_block[i]) printf(" %i:%" PRIu32, i, inode.i_block[i]);
  }
  if (inode.i_block_1ind) printf(" 1-IND:%" PRIu32, inode.i_block_1ind);
  if (inode.i_block_2ind) printf(" 2-IND:%" PRIu32, inode.i_block_2ind);
  if (inode.i_block_3ind) printf(" 3-IND:%" PRIu32, inode.i_block_3ind);
  printf(")\n");
  num_spaces += 2;
  follow_directory_entries(volume, &inode, volume, NULL, print_dir_entry_recursive);
  num_spaces -= 2;
  return 0;
}

int main(int argc, char *argv[]) {
  
  volume_t *volume;
  
  if (argc != 2) {
    fprintf(stderr, "Usage: %s volume_file\n", argv[0]);
    return 1;
  }

  errno = 0;
  volume = open_volume_file(argv[1]);
  if (!volume) {
    fprintf(stderr, "Provided volume file is invalid or incomplete: %s.\n", argv[1]);
    if (errno != 0)
      fprintf(stderr, "\t%s\n", strerror(errno));
    return 1;
  }

  printf("Volume name           : %s\n", volume->super.s_volume_name);
  printf("Last mounted on       : %s\n", volume->super.s_last_mounted);
  printf("File system version   : %" PRIu32 ".%" PRIu16 "\n",
	 volume->super.s_rev_level, volume->super.s_minor_rev_level);
  
  printf("Total size (in bytes) : %" PRIu32 "\n", volume->volume_size);
  printf("Block size (in bytes) : %" PRIu32 "\n", volume->block_size);
  printf("Total number of blocks: %" PRIu32 "\n", volume->super.s_blocks_count);
  printf("Total number of inodes: %" PRIu32 "\n", volume->super.s_inodes_count);
  printf("No of reserved blocks : %" PRIu32 "\n", volume->super.s_r_blocks_count);
  printf("Blocks per group      : %" PRIu32 "\n", volume->super.s_blocks_per_group);
  printf("Inodes per group      : %" PRIu32 "\n", volume->super.s_inodes_per_group);

  for (int g = 0; g < volume->num_groups; g++) {
    
    printf("\n== BLOCK GROUP %d ==\n", g);
    printf("Number of free blocks : %" PRIu32 "\n", volume->groups[g].bg_free_blocks_count);
    printf("Number of free inodes : %" PRIu32 "\n", volume->groups[g].bg_free_inodes_count);
    printf("No of directory inodes: %" PRIu32 "\n", volume->groups[g].bg_used_dirs_count);
  }

  uint32_t inode_no;
  inode_t inode;
  
  printf("\nRoot directory:\n");
  if (!(inode_no = find_file_from_path(volume, "/", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
    printf("  Entries:\n");
    follow_directory_entries(volume, &inode, NULL, NULL, print_dir_entry);
  }
  
  printf("\nDirectory d1:\n");
  if (!(inode_no = find_file_from_path(volume, "/d1", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
    printf("  Entries:\n");
    follow_directory_entries(volume, &inode, NULL, NULL, print_dir_entry);
  }
  
  printf("\nDirectory d1/d2:\n");
  if (!(inode_no = find_file_from_path(volume, "/d1/d2", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
    printf("  Entries:\n");
    follow_directory_entries(volume, &inode, NULL, NULL, print_dir_entry);
  }
  
  printf("\nDirectory d1/d2/d3/d4/d5:\n");
  if (!(inode_no = find_file_from_path(volume, "/d1/d2/d3/d4/d5", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
    printf("  Entries:\n");
    follow_directory_entries(volume, &inode, NULL, NULL, print_dir_entry);
  }
  
  printf("\nFile termcap:\n");
  if (!(inode_no = find_file_from_path(volume, "/termcap", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
  }
  
  printf("\nFile d1/File1.txt:\n");
  if (!(inode_no = find_file_from_path(volume, "/d1/File1.txt", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
    printf("  Content     : ");

    char content[64];
    int rv = read_file_content(volume, &inode, 0, 63, content);
    printf("%.*s\n", rv, content);
  }
  
  printf("\nFile d1/d2/sparse/Bigfile2.txt:\n");
  if (!(inode_no = find_file_from_path(volume, "/d1/d2/sparse/Bigfile2.txt", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
  }
  
  printf("\nSymlink ImageInst.txt:\n");
  if (!(inode_no = find_file_from_path(volume, "/ImageInst.txt", &inode))) {
    printf("  NOT FOUND!!!\n");
  } else {
    printf("  Inode number: %#" PRIx32 "\n", inode_no);
    printf("  Mode        : %#" PRIo32 "\n", inode.i_mode);
    printf("  Size        : %" PRIu64 "\n", inode_file_size(volume, &inode));
    printf("  Blocks      : %" PRIu32 "\n", inode.i_blocks);
  }

  printf("\nFull list of files:\n");
  print_dir_entry_recursive("", EXT2_ROOT_INO, volume);
  
  return 0;
}
