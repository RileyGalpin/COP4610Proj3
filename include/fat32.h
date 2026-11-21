#ifndef FAT32_H
#define FAT32_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_IMAGE_NAME 256   /* longest image name kept for the shell prompt */
#define MAX_PATH_LEN   4096  /* longest working-directory path we track */

/* FAT32 FAT entries only use the low 28 bits (spec section 4). */
#define FAT_ENTRY_MASK  0x0FFFFFFFu
#define FAT_BAD_CLUSTER 0x0FFFFFF7u
#define FAT_EOC_MIN     0x0FFFFFF8u  /* values >= this mark end of chain */

/*
 * On-disk layout of the boot sector (BPB) up to and including BS_FilSysType.
 * All multi-byte fields are little endian, which matches the host (x86), so the
 * sector can be read straight into this struct.
 */
typedef struct __attribute__((packed)) {
    uint8_t  BS_jmpBoot[3];
    uint8_t  BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t  BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t  BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t  BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t  BPB_Reserved[12];
    uint8_t  BS_DrvNum;
    uint8_t  BS_Reserved1;
    uint8_t  BS_BootSig;
    uint32_t BS_VolID;
    uint8_t  BS_VolLab[11];
    uint8_t  BS_FilSysType[8];
} BootSector;

/* Compile-time check that the struct matches the 90-byte on-disk layout. */
typedef char boot_sector_size_check[(sizeof(BootSector) == 90) ? 1 : -1];

/* Everything the shell needs to know about the mounted image. */
typedef struct {
    FILE *fp;                       /* open image file */
    char name[MAX_IMAGE_NAME];      /* image file name, shown in the prompt */
    BootSector bs;                  /* raw boot sector fields */

    /* Values derived from the boot sector at mount time. */
    uint64_t image_size;            /* size of the image file in bytes */
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t total_sectors;
    uint32_t num_fats;
    uint32_t fat_size_sectors;      /* sectors occupied by one FAT */
    uint64_t entries_per_fat;       /* FAT entries that fit in one FAT */
    uint32_t count_of_clusters;     /* clusters in the data region */
    uint32_t root_cluster;          /* first cluster of the root directory */
    uint64_t fat_offset;            /* byte offset of the first FAT */
    uint64_t data_offset;           /* byte offset of cluster 2 */

    /* Shell state. */
    uint32_t cwd_cluster;           /* first cluster of the working directory */
    char cwd_path[MAX_PATH_LEN];    /* "" for root, else "/DIR/SUBDIR" */
} Fat32Image;

/*
 * Opens and validates the image at path. On failure prints an error message
 * and returns -1; on success returns 0 and the working directory is the root.
 */
int fat32_mount(Fat32Image *img, const char *path);

/* Closes the image and releases anything held by img. Safe to call twice. */
void fat32_unmount(Fat32Image *img);

/* Reads len bytes at a byte offset in the image. Prints an error, returns -1 on failure. */
int fat32_read(const Fat32Image *img, uint64_t offset, void *buf, size_t len);

/* True if cluster is a valid data-region cluster number (2 .. count + 1). */
bool fat32_valid_cluster(const Fat32Image *img, uint32_t cluster);

/* Byte offset in the image of the first byte of a (valid) data cluster. */
uint64_t fat32_cluster_offset(const Fat32Image *img, uint32_t cluster);

/*
 * Reads the first FAT's entry for cluster (the high 4 bits are masked off).
 * Prints an error and returns -1 on failure.
 */
int fat32_read_fat_entry(const Fat32Image *img, uint32_t cluster, uint32_t *value);

/* True if a FAT entry value marks the end of a cluster chain. */
bool fat32_is_end_of_chain(uint32_t fat_value);

#endif
