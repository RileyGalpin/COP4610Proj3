#include "commands.h"
#include "directory.h"

#include <stdio.h>
#include <string.h>

/* Appends "/name" to the working-directory path. Returns -1 if it would not fit. */
static int path_push(char *path, const char *name)
{
    size_t len = strlen(path);
    int written = snprintf(path + len, MAX_PATH_LEN - len, "/%s", name);

    if (written < 0 || (size_t)written >= MAX_PATH_LEN - len) {
        path[len] = '\0';
        printf("Error: path is too long\n");
        return -1;
    }
    return 0;
}

/* Removes the last component of the working-directory path ("/A/B" -> "/A"). */
static void path_pop(char *path)
{
    char *slash = strrchr(path, '/');

    if (slash != NULL)
        *slash = '\0';
}

/*
 * cd [DIRNAME]: change the working directory to a directory in the current one.
 * The path and cluster are only updated after every check has passed.
 */
int cmd_cd(Fat32Image *img, tokenlist *args)
{
    const char *name = args->items[1];
    char entry_name[DIR_NAME_BUF];
    DirEntry entry;
    uint32_t cluster;
    bool is_dot;
    bool is_dotdot;
    int rc;

    /* The root directory has no "." or ".." entries, so staying put is handled here. */
    if (img->cwd_cluster == img->root_cluster &&
        (strcmp(name, ".") == 0 || strcmp(name, "..") == 0))
        return CMD_CONTINUE;

    rc = dir_find(img, img->cwd_cluster, name, &entry, NULL);
    if (rc < 0)
        return CMD_CONTINUE;  /* error already reported */
    if (rc == 0) {
        printf("Error: '%s' does not exist\n", name);
        return CMD_CONTINUE;
    }
    if (!dir_entry_is_directory(&entry)) {
        printf("Error: '%s' is not a directory\n", name);
        return CMD_CONTINUE;
    }

    dir_entry_name(&entry, entry_name);
    is_dot = strcmp(entry_name, ".") == 0;
    is_dotdot = strcmp(entry_name, "..") == 0;

    cluster = dir_entry_cluster(&entry);
    /* A ".." entry stores cluster 0 when the parent is the root directory. */
    if (is_dotdot && cluster == 0)
        cluster = img->root_cluster;

    if (!fat32_valid_cluster(img, cluster)) {
        printf("Error: directory '%s' has an invalid first cluster (%u)\n", name, cluster);
        return CMD_CONTINUE;
    }

    if (is_dotdot)
        path_pop(img->cwd_path);
    else if (!is_dot && path_push(img->cwd_path, entry_name) != 0)
        return CMD_CONTINUE;

    img->cwd_cluster = cluster;
    return CMD_CONTINUE;
}

/*
 * ls: print the name of every file and directory in the working directory, one
 * per line. Long-name pieces, deleted entries and the volume label are skipped.
 * "." and ".." appear because subdirectories store them as real entries.
 */
int cmd_ls(Fat32Image *img, tokenlist *args)
{
    char entry_name[DIR_NAME_BUF];
    DirEntry entry;
    DirIter it;
    uint64_t offset;

    (void)args;

    dir_iter_init(&it, img, img->cwd_cluster);
    while (dir_iter_next(&it, &entry, &offset) == 1) {
        if (!dir_entry_is_listed(&entry))
            continue;
        dir_entry_name(&entry, entry_name);
        printf("%s\n", entry_name);
    }
    return CMD_CONTINUE;
}
