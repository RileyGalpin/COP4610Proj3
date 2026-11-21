SRC := src
OBJ := obj
BIN := bin
EXECUTABLE := filesys

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
INCS := -Iinclude/
DIRS := $(OBJ)/ $(BIN)/
EXEC := $(BIN)/$(EXECUTABLE)

CC := gcc
# -MMD -MP make gcc write .d files so objects are rebuilt when a header changes
CFLAGS := -g -Wall -std=c99 -MMD -MP $(INCS)
LDFLAGS :=

# Image mounted by "make run"; override with: make run IMG=path/to/image
IMG ?= fat32.img

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(EXEC)
	$(EXEC) $(IMG)

clean:
	rm -f $(OBJ)/*.o $(OBJ)/*.d $(EXEC)

$(shell mkdir -p $(DIRS))

-include $(DEPS)

.PHONY: run clean all
