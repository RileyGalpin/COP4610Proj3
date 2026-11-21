#include "directory.h"

#include <ctype.h>

void dir_iter_init(DirIter *it, Fat32Image *img, uint32_t first_cluster)
{
    it->img = img;
    it->cluster = first_cluster;
    it->index = 0;
    it->checkpoint = first_cluster;
    it->steps = 0;
    it->power = 1;
}

int dir_iter_next(DirIter *it, DirEntry *entry, uint64_t *offset)
{
    const Fat32Image *img = it->img;
    uint32_t entries_per_cluster = img->bytes_per_cluster / DIR_ENTRY_SIZE;
    uint32_t next;
    uint64_t pos;

    if (it->cluster == 0)
        return 0;

    /* Finished this cluster: follow the FAT to the next one, if there is one. */
    if (it->index >= entries_per_cluster) {
        if (fat32_read_fat_entry(img, it->cluster, &next) != 0)
            return -1;
        if (fat32_is_end_of_chain(next)) {
            it->cluster = 0;
            return 0;
        }
        /*
         * A valid chain never visits a cluster twice, so meeting the checkpoint
         * again means the FAT loops. The checkpoint moves forward at doubling
         * intervals (Brent's algorithm), which catches any loop after only a
         * few repeated clusters.
         */
        if (next == it->checkpoint) {
            printf("Error: directory cluster chain loops back on itself\n");
            return -1;
        }
        if (++it->steps == it->power) {
            it->checkpoint = next;
            it->steps = 0;
            it->power *= 2;
        }
        it->cluster = next;
        it->index = 0;
    }

    if (!fat32_valid_cluster(img, it->cluster)) {
        printf("Error: directory uses invalid cluster %u\n", it->cluster);
        return -1;
    }

    pos = fat32_cluster_offset(img, it->cluster) + (uint64_t)it->index * DIR_ENTRY_SIZE;
    if (fat32_read(img, pos, entry, DIR_ENTRY_SIZE) != 0)
        return -1;
    it->index++;
    *offset = pos;

    if (entry->DIR_Name[0] == DIR_ENTRY_END) {
        it->cluster = 0;
        return 0;
    }
    return 1;
}

bool dir_entry_is_listed(const DirEntry *entry)
{
    if (entry->DIR_Name[0] == DIR_ENTRY_DELETED || entry->DIR_Name[0] == DIR_ENTRY_END)
        return false;
    /* Long file name pieces are skipped; only the short-name entry is used. */
    if ((entry->DIR_Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME)
        return false;
    return (entry->DIR_Attr & ATTR_VOLUME_ID) == 0;
}

bool dir_entry_is_directory(const DirEntry *entry)
{
    return (entry->DIR_Attr & ATTR_DIRECTORY) != 0;
}

uint32_t dir_entry_cluster(const DirEntry *entry)
{
    return (((uint32_t)entry->DIR_FstClusHI << 16) | entry->DIR_FstClusLO) & FAT_ENTRY_MASK;
}

void dir_entry_name(const DirEntry *entry, char out[DIR_NAME_BUF])
{
    int base_len = 8;
    int ext_len = 3;
    int len = 0;
    int i;

    /* Names are stored space padded: drop the padding from each half. */
    while (base_len > 0 && entry->DIR_Name[base_len - 1] == ' ')
        base_len--;
    while (ext_len > 0 && entry->DIR_Name[8 + ext_len - 1] == ' ')
        ext_len--;

    for (i = 0; i < base_len; i++)
        out[len++] = (char)entry->DIR_Name[i];
    if (ext_len > 0) {
        out[len++] = '.';
        for (i = 0; i < ext_len; i++)
            out[len++] = (char)entry->DIR_Name[8 + i];
    }
    out[len] = '\0';
}

/* Case-insensitive string equality (FAT short names are stored upper case). */
static bool names_equal(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

int dir_find(Fat32Image *img, uint32_t dir_cluster, const char *name,
             DirEntry *out, uint64_t *offset)
{
    DirIter it;
    DirEntry entry;
    uint64_t pos;
    char entry_name[DIR_NAME_BUF];
    int rc;

    dir_iter_init(&it, img, dir_cluster);
    while ((rc = dir_iter_next(&it, &entry, &pos)) == 1) {
        if (!dir_entry_is_listed(&entry))
            continue;
        dir_entry_name(&entry, entry_name);
        if (names_equal(entry_name, name)) {
            *out = entry;
            if (offset != NULL)
                *offset = pos;
            return 1;
        }
    }
    return rc;  /* 0 = not found, -1 = error already reported */
}
