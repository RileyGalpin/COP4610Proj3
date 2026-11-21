#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "fat32.h"

#define DIR_ENTRY_SIZE 32
#define DIR_NAME_BUF   13  /* "NAME" + '.' + "EXT" + NUL */

/* Directory entry attribute bits (spec section 6.2). */
#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_NAME      (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define ATTR_LONG_NAME_MASK (ATTR_LONG_NAME | ATTR_DIRECTORY | ATTR_ARCHIVE)

#define DIR_ENTRY_DELETED 0xE5  /* first name byte of a deleted entry */
#define DIR_ENTRY_END     0x00  /* first name byte: this and all later entries are unused */

/* On-disk 32-byte short-name directory entry (spec section 6). */
typedef struct __attribute__((packed)) {
    uint8_t  DIR_Name[11];
    uint8_t  DIR_Attr;
    uint8_t  DIR_NTRes;
    uint8_t  DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} DirEntry;

/* Compile-time check that the struct matches the on-disk entry size. */
typedef char dir_entry_size_check[(sizeof(DirEntry) == DIR_ENTRY_SIZE) ? 1 : -1];

/* Walks the entries of one directory, following its cluster chain through the FAT. */
typedef struct {
    Fat32Image *img;
    uint32_t cluster;     /* cluster being scanned; 0 once the walk has finished */
    uint32_t index;       /* next entry to read within that cluster */

    /* Brent's cycle detection, so a corrupt FAT that loops cannot hang the walk. */
    uint32_t checkpoint;  /* an earlier cluster of this chain */
    uint32_t steps;       /* clusters followed since the checkpoint was set */
    uint32_t power;       /* steps allowed before the checkpoint moves forward */
} DirIter;

/* Starts a walk at the directory whose first cluster is first_cluster (never 0). */
void dir_iter_init(DirIter *it, Fat32Image *img, uint32_t first_cluster);

/*
 * Produces the next raw entry, including deleted and long-name entries; use
 * dir_entry_is_listed() to filter. *offset receives the entry's byte offset in
 * the image, which is what a later update of that entry needs.
 *
 * Returns 1 when an entry was produced, 0 at the end of the directory (the end
 * marker entry, or the last cluster), and -1 after printing an error. When the
 * end is an end marker, *offset is that marker's offset (a free slot).
 */
int dir_iter_next(DirIter *it, DirEntry *entry, uint64_t *offset);

/* True for an ordinary file or directory: not deleted, not a long-name or volume label. */
bool dir_entry_is_listed(const DirEntry *entry);

bool dir_entry_is_directory(const DirEntry *entry);

/* First data cluster of the entry (0 for an empty file or a ".." pointing at root). */
uint32_t dir_entry_cluster(const DirEntry *entry);

/* Formats the 8.3 name as "NAME" or "NAME.EXT" with padding removed. */
void dir_entry_name(const DirEntry *entry, char out[DIR_NAME_BUF]);

/*
 * Looks name up (case-insensitively) among the listed entries of the directory
 * starting at dir_cluster. offset may be NULL.
 * Returns 1 and fills *out if found, 0 if not found, -1 after printing an error.
 */
int dir_find(Fat32Image *img, uint32_t dir_cluster, const char *name,
             DirEntry *out, uint64_t *offset);

#endif
