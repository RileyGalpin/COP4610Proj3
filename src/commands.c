#include "commands.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    CommandHandler handler;
    size_t min_args;    /* argument counts exclude the command name itself */
    size_t max_args;
    const char *usage;  /* shown when the argument count is wrong */
} Command;

/* To add a command: write its handler, declare it in commands.h, add a row here. */
static const Command command_table[] = {
    { "info", cmd_info, 0, 0, "info" },
    { "exit", cmd_exit, 0, 0, "exit" },
    { "cd",   cmd_cd,   1, 1, "cd [DIRNAME]" },
    { "ls",   cmd_ls,   0, 0, "ls" },
};

int run_command(Fat32Image *img, tokenlist *tokens)
{
    size_t table_size = sizeof(command_table) / sizeof(command_table[0]);
    size_t nargs;
    size_t i;

    if (tokens->size == 0)
        return CMD_CONTINUE;

    for (i = 0; i < table_size; i++) {
        const Command *cmd = &command_table[i];

        if (strcmp(tokens->items[0], cmd->name) != 0)
            continue;

        nargs = tokens->size - 1;
        if (nargs < cmd->min_args || nargs > cmd->max_args) {
            printf("Error: wrong number of arguments. Usage: %s\n", cmd->usage);
            return CMD_CONTINUE;
        }
        return cmd->handler(img, tokens);
    }

    printf("Error: unknown command '%s'\n", tokens->items[0]);
    return CMD_CONTINUE;
}
