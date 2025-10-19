TARGET_EXEC := selection_sort.elf

BUILD_DIR := ./build
SRC_DIRS := ./src

SRCS := $(shell find $(SRC_DIRS) -name '*.c' -or -name '*.s')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS := $(INC_FLAGS) -nostdlib -march=rv32i -mabi=ilp32 -g

CC := riscv32-unknown-none-elf-gcc

RV32_EMU := rv32-emu

GDB := riscv32-unknown-none-elf-gdb
GDB_PORT := 3333

all: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.s.o: %.s
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BUILD_DIR)/$(TARGET_EXEC)
	$(RV32_EMU) $(RV32_EMU_FLAGS) $<

listen: $(BUILD_DIR)/$(TARGET_EXEC)
	$(RV32_EMU) $(RV32_EMU_FLAGS) -l -p $(GDB_PORT) $<

debug: $(BUILD_DIR)/$(TARGET_EXEC)
	$(GDB) $(GDB_FLAGS) $<

clean:
	rm -r $(BUILD_DIR)

.PHONY: all clean run debug
