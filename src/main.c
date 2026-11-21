#include <stdio.h>
#include <stdlib.h>

#include "../include/commands.h"
#include "../include/fat32.h"
#include "../include/lexer.h"

/* Prompt looks like "fat32.img/>" at the root and "fat32.img/BLUE/>" inside BLUE. */
static void print_prompt(const Fat32Image *img)
{
    printf("%s%s/> ", img->name, img->cwd_path);
    fflush(stdout);
}

/* Reads and runs commands until "exit" or end of input. */
static void shell_loop(Fat32Image *img)
{
    int status = CMD_CONTINUE;

    while (status != CMD_QUIT) {
        print_prompt(img);

        char *input = get_input();
        if (input == NULL) {  /* end of input, e.g. Ctrl-D */
            printf("\n");
            break;
        }

        tokenlist *tokens = get_tokens(input);
        status = run_command(img, tokens);

        free(input);
        free_tokens(tokens);
    }
}

int main(int argc, char *argv[])
{
    Fat32Image img;

    if (argc != 2) {
        printf("Usage: %s [FAT32 ISO]\n", argv[0]);
        return 1;
    }

    if (fat32_mount(&img, argv[1]) != 0)
        return 1;

    shell_loop(&img);

    fat32_unmount(&img);
    return 0;
}
