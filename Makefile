override CC = gcc

CFLAGS = -std=c23 -g -MMD -MP -Werror -Wall -Wextra -Wpedantic \
-Wmissing-prototypes -Wstrict-prototypes -Wsign-compare -Wswitch \
-Wconversion -Wshadow -Wvla

GCCFLAGS = -Wmaybe-uninitialized -Wfree-nonheap-object -Wformat-signedness \
# LLVMFLAGS = -Wsometimes-uninitialized

SANFLAGS = -fsanitize=address,undefined -fno-sanitize-recover=undefined

SRC_DIR = src
BUILD_DIR = build
INCLUDES = -I$(SRC_DIR)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
TARGET = $(BUILD_DIR)/kramac

SAN_BUILD_DIR = $(BUILD_DIR)/san
SAN_OBJS = $(patsubst $(SRC_DIR)/%.c, $(SAN_BUILD_DIR)/%.o, $(SRCS))
SAN_TARGET = $(BUILD_DIR)/kramac-san

TEST_ROOT = tests
TEST_DIR = $(TEST_ROOT)/unit_tests
TEST_BUILD_DIR = $(TEST_ROOT)/build
TEST_INCLUDES = $(INCLUDES) -I$(TEST_DIR)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
SRC_TEST_OBJS = $(patsubst $(BUILD_DIR)/%, $(TEST_BUILD_DIR)/%, $(OBJS))
TEST_OBJS = $(filter-out $(TEST_BUILD_DIR)/main.o,$(SRC_TEST_OBJS))
TEST_OBJS +=	$(patsubst $(TEST_DIR)/%.c, $(TEST_BUILD_DIR)/%.o, $(TEST_SRCS))
TEST_TARGET = $(TEST_BUILD_DIR)/test
TEST_MACRO = -DKRAMA_TEST_SUITE

FMT_DIRS := $(SRC_DIR) $(TEST_ROOT)
FMT_FILES := $(shell find $(FMT_DIRS) -type f \( -name "*.c" -o -name "*.h" \))

all: $(TARGET)

test: $(TEST_TARGET)

san_build: $(SAN_TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) $(SANFLAGS) -o $@ $(TEST_OBJS)

$(SAN_TARGET): $(SAN_OBJS)
	$(CC) $(SANFLAGS) -o $@ $(SAN_OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(GCCFLAGS) -c $< -o $@

$(SAN_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(SAN_BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(GCCFLAGS) $(SANFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_MACRO) $(TEST_INCLUDES) $(GCCFLAGS) $(SANFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_MACRO) $(TEST_INCLUDES) $(GCCFLAGS) $(SANFLAGS) -c $< -o $@

.PHONY: all clean test clean_test fmt fmt_check check san_build
clean:
	rm -rf $(BUILD_DIR)

clean_test:
	rm -rf $(TEST_BUILD_DIR) $(SAN_BUILD_DIR)

fmt:
	clang-format -i $(FMT_FILES)

fmt_check:
	@clang-format --dry-run --Werror $(FMT_FILES)

# TODO: Run the sanitized build once the harness is in
check: fmt_check all test san_build
	$(TEST_TARGET)

-include $(OBJS:.o=.d) $(SAN_OBJS:.o=.d) $(TEST_OBJS:.o=.d)
