override CC = gcc

CFLAGS = -std=c23 -g -MMD -MP -Werror -Wall -Wextra -Wpedantic \
-Wmissing-prototypes -Wstrict-prototypes -Wsign-compare -Wswitch \
-Wconversion -Wshadow -Wvla

GCCFLAGS = -Wmaybe-uninitialized -Wfree-nonheap-object -Wformat-signedness \
# LLVMFLAGS = -Wsometimes-uninitialized

SANFLAGS = -fsanitize=address,undefined

SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

TARGET = $(BUILD_DIR)/cfunC

SRCS = $(wildcard $(SRC_DIR)/*.c)

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

ifneq ($(filter test,$(MAKECMDGOALS)),)
	BUILD_DIR := $(TEST_DIR)/$(BUILD_DIR)
	TEST_TARGET := $(BUILD_DIR)/test
	SRCS := $(filter-out $(SRC_DIR)/main.c,$(SRCS))
	TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
	TEST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/%.o, $(TEST_SRCS))
	OBJS := $(OBJS) $(TEST_OBJS)
	GCCFLAGS := $(GCCFLAGS) $(SANFLAGS)
	CFLAGS := $(CFLAGS) -DFUNC_TEST_SUITE
endif

all: $(TARGET)

test: $(TEST_TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

$(TEST_TARGET): $(OBJS)
	$(CC) $(SANFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GCCFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GCCFLAGS) -c $< -o $@

.PHONY: all clean
clean:
	rm -rf $(BUILD_DIR)

clean_test:
	rm -rf $(TEST_DIR)/$(BUILD_DIR)

# test:
# 	@echo "Test dir: $(TEST_DIR)"
# 	@echo "Source Dir: $(SRC_DIR)"
# 	@echo "Build Dir: $(BUILD_DIR)"
# 	@echo "Target : $(TARGET)"
# 	@echo "Objects: $(OBJS)"
# 	@echo "Test Sources: $(TEST_SRCS)"

-include $(OBJS:.o=.d)
