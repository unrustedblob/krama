
CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -Wsign-compare -Wswitch -Wconversion -Wshadow -Wvla -Wmaybe-uninitialized -Wfree-nonheap-object

BUILD_DIR = build
SRC_DIR = src

TARGET = ${BUILD_DIR}/cfunC

SRCS = $(wildcard ${SRC_DIR}/*.c)

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p ${BUILD_DIR}
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY:
clean:
	rm -rf build
