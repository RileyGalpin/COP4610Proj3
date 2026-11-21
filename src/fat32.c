#include "fat32.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#define SECTOR0_SIZE 512  /* the BPB always fits in the first 512 bytes */

/* Reports a malformed image. Always returns -1 so callers can `return reject(...)`. */
static int reject(const char *path, const char *reason)
{
    printf("Error: '%s' is not a valid FAT32 image (%s)\n", path, reason);
    return -1;
}

/*
 * Reads and validates the boot sector, then fills in the derived fields of img.
 * Every value later used for offset arithmetic is range-checked here so the
 * rest of the program can trust it.
 */
static int parse_boot_sector(Fat32Image *img, const char *path)
{
    uint8_t sector[SECTOR0_SIZE];
    const BootSector *bs = &img->bs;
    uint64_t total, overhead, clusters;
    uint32_t bps, spc;
    long size;

    if (fseek(img->fp, 0, SEEK_END) != 0 || (size = ftell(img->fp)) < 0) {
        printf("Error: cannot determine the size of '%s'\n", path);
        return -1;
    }
    img->image_size = (uint64_t)size;
    if (img->image_size < SECTOR0_SIZE)
        return reject(path, "file is smaller than one sector");

    if (fat32_read(img, 0, sector, sizeof(sector)) != 0)
        return -1;
    memcpy(&img->bs, sector, sizeof(BootSector));

    if (sector[510] != 0x55 || sector[511] != 0xAA)
        return reject(path, "missing 0x55AA boot sector signature");

    bps = bs->BPB_BytsPerSec;
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096)
        return reject(path, "unsupported bytes per sector");

    spc = bs->BPB_SecPerClus;
    if (spc == 0 || spc > 128 || (spc & (spc - 1)) != 0)
        return reject(path, "sectors per cluster is not a power of 2 up to 128");

    if (bs->BPB_RsvdSecCnt == 0 || bs->BPB_NumFATs == 0)
        return reject(path, "no reserved sectors or no FATs");

    /* FAT12/16 volumes have a fixed root directory and a 16-bit FAT size. */
    if (bs->BPB_RootEntCnt != 0 || bs->BPB_FATSz16 != 0 || bs->BPB_FATSz32 == 0)
        return reject(path, "volume is FAT12/FAT16, not FAT32");

    total = bs->BPB_TotSec16 != 0 ? bs->BPB_TotSec16 : bs->BPB_TotSec32;
    if (total == 0)
        return reject(path, "total sector count is zero");

    overhead = bs->BPB_RsvdSecCnt + (uint64_t)bs->BPB_NumFATs * bs->BPB_FATSz32;
    if (overhead >= total)
        return reject(path, "reserved region and FATs do not fit in the volume");

    clusters = (total - overhead) / spc;
    if (clusters == 0 || clusters + 1 >= FAT_BAD_CLUSTER)
        return reject(path, "invalid number of data clusters");

    if ((uint64_t)bs->BPB_FATSz32 * bps / 4 < clusters + 2)
        return reject(path, "FAT is too small for the number of clusters");

    if (bs->BPB_RootClus < 2 || bs->BPB_RootClus > clusters + 1)
        return reject(path, "root directory cluster is out of range");

    if (img->image_size < total * bps)
        return reject(path, "file is smaller than the volume the boot sector describes");

    img->bytes_per_sector = bps;
    img->sectors_per_cluster = spc;
    img->bytes_per_cluster = bps * spc;
    img->total_sectors = (uint32_t)total;
    img->num_fats = bs->BPB_NumFATs;
    img->fat_size_sectors = bs->BPB_FATSz32;
    img->entries_per_fat = (uint64_t)bs->BPB_FATSz32 * bps / 4;
    img->count_of_clusters = (uint32_t)clusters;
    img->root_cluster = bs->BPB_RootClus;
    img->fat_offset = (uint64_t)bs->BPB_RsvdSecCnt * bps;
    img->data_offset = overhead * bps;
    return 0;
}

int fat32_mount(Fat32Image *img, const char *path)
{
    const char *base;

    memset(img, 0, sizeof(*img));

    img->fp = fopen(path, "r+b");
    if (img->fp == NULL) {
        if (errno == ENOENT)
            printf("Error: image file '%s' does not exist\n", path);
        else
            printf("Error: cannot open image file '%s': %s\n", path, strerror(errno));
        return -1;
    }

    if (parse_boot_sector(img, path) != 0) {
        fclose(img->fp);
        img->fp = NULL;
        return -1;
    }

    /* The prompt shows only the file name, not the directories leading to it. */
    base = strrchr(path, '/');
    snprintf(img->name, sizeof(img->name), "%s", base != NULL ? base + 1 : path);

    img->cwd_cluster = img->root_cluster;
    img->cwd_path[0] = '\0';
    return 0;
}

void fat32_unmount(Fat32Image *img)
{
    if (img->fp != NULL) {
        fclose(img->fp);
        img->fp = NULL;
    }
}

int fat32_read(const Fat32Image *img, uint64_t offset, void *buf, size_t len)
{
    if (offset > img->image_size || len > img->image_size - offset) {
        printf("Error: read of %zu bytes at offset %llu is past the end of the image\n",
               len, (unsigned long long)offset);
        return -1;
    }
    if (offset > LONG_MAX || fseek(img->fp, (long)offset, SEEK_SET) != 0 ||
        fread(buf, 1, len, img->fp) != len) {
        printf("Error: failed to read image at offset %llu\n", (unsigned long long)offset);
        return -1;
    }
    return 0;
}

bool fat32_valid_cluster(const Fat32Image *img, uint32_t cluster)
{
    return cluster >= 2 && cluster <= (uint64_t)img->count_of_clusters + 1;
}

uint64_t fat32_cluster_offset(const Fat32Image *img, uint32_t cluster)
{
    return img->data_offset + (uint64_t)(cluster - 2) * img->bytes_per_cluster;
}

int fat32_read_fat_entry(const Fat32Image *img, uint32_t cluster, uint32_t *value)
{
    uint8_t raw[4];

    if (!fat32_valid_cluster(img, cluster)) {
        printf("Error: cluster %u is outside the data region\n", cluster);
        return -1;
    }
    if (fat32_read(img, img->fat_offset + (uint64_t)cluster * 4, raw, sizeof(raw)) != 0)
        return -1;

    /* Little endian decode; the top 4 bits are reserved and must be ignored. */
    *value = ((uint32_t)raw[0] | (uint32_t)raw[1] << 8 | (uint32_t)raw[2] << 16 |
              (uint32_t)raw[3] << 24) & FAT_ENTRY_MASK;
    return 0;
}

bool fat32_is_end_of_chain(uint32_t fat_value)
{
    return fat_value >= FAT_EOC_MIN;
}
