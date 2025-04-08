TARGET_EXEC := dm-utils

BUILD_DIR := ./build

SRCS := \
	main.c \
	libdm.c \
	argtable3/argtable3.c \

INCLUDES := \
	argtable3 \

CFLAGS := -o2 -g

LIBS := m

SRCS := $(addprefix src/,$(SRCS))

OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

INC_FLAGS := $(addprefix -Isrc/,$(INCLUDES))

LIB_FLAGS := $(addprefix -l,$(LIBS))

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIB_FLAGS)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)
