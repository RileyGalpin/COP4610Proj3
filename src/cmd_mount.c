#include "commands.h"

#include <stdio.h>

/* info: print the boot sector fields the project asks for, one per line. */
int cmd_info(Fat32Image *img, tokenlist *args)
{
    (void)args;

    printf("Position of root cluster (in cluster #): %u\n", img->root_cluster);
    printf("Bytes per sector: %u\n", img->bytes_per_sector);
    printf("Sectors per cluster: %u\n", img->sectors_per_cluster);
    printf("Total # of clusters in data region: %u\n", img->count_of_clusters);
    printf("# of entries in one FAT: %llu\n", (unsigned long long)img->entries_per_fat);
    printf("Size of image (in bytes): %llu\n", (unsigned long long)img->image_size);
    return CMD_CONTINUE;
}

/*
 * exit: ask the shell loop to stop. The image is closed by main() once the loop
 * ends, so end-of-input (Ctrl-D) and "exit" share the same cleanup path.
 */
int cmd_exit(Fat32Image *img, tokenlist *args)
{
    (void)img;
    (void)args;
    return CMD_QUIT;
}
