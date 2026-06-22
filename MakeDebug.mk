BUILD_DIR = debug
TARGET = $(BUILD_DIR)/dcc
SRCS = $(shell find src -name '*.c')
OBJS_DIR = $(BUILD_DIR)/obj
OBJS = $(patsubst src/%.c,$(OBJS_DIR)/%.o,$(SRCS))

.PHONY: all

all: $(TARGET)

CFLAGS += -g
CFLAGS += -DDEBUG

$(TARGET): $(OBJS)
	@echo "Building" $@
	gcc $(CFLAGS) -o $@ $^

$(OBJS_DIR)/%.o: src/%.c
	@echo "[Compile Object]" $< "->" $@
	gcc -c $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	@rm $(TARGET)
	@rm $(OBJS_DIR)/*

.PHONY: nuke
nuke:
	@rm -rf $(BUILD_DIR)
