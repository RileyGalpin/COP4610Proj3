#ifndef COMMANDS_H
#define COMMANDS_H

#include "fat32.h"
#include "lexer.h"

/* What a command handler returns to the shell loop. */
#define CMD_CONTINUE 0  /* keep reading commands */
#define CMD_QUIT     1  /* leave the shell */

/*
 * A command handler. args->items[0] is the command name and the remaining
 * items are its arguments; the dispatcher has already checked the count.
 * Handlers print their own error messages and never leave the image half-updated.
 */
typedef int (*CommandHandler)(Fat32Image *img, tokenlist *args);

/* Looks up tokens->items[0] and runs it. Blank input does nothing. */
int run_command(Fat32Image *img, tokenlist *tokens);

/* Part 1: mounting (cmd_mount.c) */
int cmd_info(Fat32Image *img, tokenlist *args);
int cmd_exit(Fat32Image *img, tokenlist *args);

/* Part 2: navigation (cmd_navigate.c) */
int cmd_cd(Fat32Image *img, tokenlist *args);
int cmd_ls(Fat32Image *img, tokenlist *args);

#endif
