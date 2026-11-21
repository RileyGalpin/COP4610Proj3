#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reads one line from stdin without its newline.
 * Returns NULL at end of input (EOF) when no characters were read. */
char *get_input(void) {
	char *buffer = NULL;
	size_t bufsize = 0;
	char line[128];

	while (fgets(line, sizeof(line), stdin) != NULL)
	{
		size_t len = strlen(line);
		int end_of_line = (len > 0 && line[len - 1] == '\n');

		if (end_of_line) {
			len--;
			if (len > 0 && line[len - 1] == '\r')
				len--;
		}

		char *grown = (char *)realloc(buffer, bufsize + len + 1);
		if (grown == NULL) {
			free(buffer);
			return NULL;
		}
		buffer = grown;
		memcpy(&buffer[bufsize], line, len);
		bufsize += len;
		buffer[bufsize] = '\0';

		if (end_of_line)
			return buffer;
	}

	/* EOF: return a final line that had no newline, otherwise signal EOF. */
	if (bufsize == 0) {
		free(buffer);
		return NULL;
	}
	return buffer;
}

tokenlist *new_tokenlist(void) {
	tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **)malloc(sizeof(char *));
	tokens->items[0] = NULL; /* make NULL terminated */
	return tokens;
}

void add_token(tokenlist *tokens, char *item) {
	int i = tokens->size;

	tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
	tokens->items[i] = (char *)malloc(strlen(item) + 1);
	tokens->items[i + 1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
	char *buf = (char *)malloc(strlen(input) + 1);
	strcpy(buf, input);
	tokenlist *tokens = new_tokenlist();
	char *tok = strtok(buf, " \t");
	while (tok != NULL)
	{
		add_token(tokens, tok);
		tok = strtok(NULL, " \t");
	}
	free(buf);
	return tokens;
}

void free_tokens(tokenlist *tokens) {
	for (size_t i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}
