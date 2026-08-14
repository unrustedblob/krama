override CC = gcc

CFLAGS = -std=c23 -g -MMD -MP -Werror -Wall -Wextra -Wpedantic \
-Wmissing-prototypes -Wstrict-prototypes -Wsign-compare -Wswitch \
-Wconversion -Wshadow -Wvla

GCCFLAGS = -Wmaybe-uninitialized -Wfree-nonheap-object -Wformat-signedness
# LLVMFLAGS = -Wsometimes-uninitialized

BUILD_DIR = build
SRC_DIR = src

TARGET = $(BUILD_DIR)/cfunC

SRCS = $(wildcard $(SRC_DIR)/*.c)

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GCCFLAGS) -c $< -o $@


.PHONY: all clean
clean:
	rm -rf $(BUILD_DIR)

-include $(OBJS:.o=.d)
