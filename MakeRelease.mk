BUILD_DIR = release
TARGET = $(BUILD_DIR)/dcc
SRCS = $(shell find src -name '*.c')
OBJS_DIR = $(BUILD_DIR)/obj
OBJS = $(patsubst src/%.c,$(OBJS_DIR)/%.o,$(SRCS))
DEPENDS = $(OBJS:.o=.d)

.PHONY: all

all: $(TARGET)

CFLAGS += -O3 -MMD -MP

$(TARGET): $(OBJS)
	@echo "Building" $@
	gcc $(CFLAGS) -o $@ $^

$(OBJS_DIR)/%.o: src/%.c
	@echo "[Compile Object]" $< "->" $@
	gcc -c $(CFLAGS) -o $@ $<

-include $(DEPENDS)

.PHONY: clean
clean:
	@rm $(OBJS_DIR)/*
	@rm $(TARGET)

.PHONY: nuke
nuke:
	@rm -rf $(BUILD_DIR)
