SRC := src
OBJ := obj
BIN := bin
EXECUTABLE:= filesys

SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
INCS := -Iinclude/
DIRS := $(OBJ)/ $(BIN)/
EXEC := $(BIN)/$(EXECUTABLE)

CC := gcc
CFLAGS := -g -Wall -std=c99 $(INCS)
LDFLAGS :=

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(EXEC)
	$(EXEC)

clean:
	-rm -f $(OBJ)/*.o $(EXEC)

$(shell mkdir -p $(DIRS))

.PHONY: run clean all

