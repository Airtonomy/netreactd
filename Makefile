BUILD_DIR := _build
OUT := $(BUILD_DIR)/netreact
CFLAGS := -O -Wall
CC := gcc
LDFLAGS :=
LDLIBS := -lpthread

CFILES := $(wildcard **.c)
OBJFILES := $(foreach file,$(CFILES),$(BUILD_DIR)/$(file:.c=.o))

.PHONY: build
build: $(OUT)

.PHONY: run
run: $(OUT)
	$(OUT)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: help
help:
	@echo "Logical targets:"
	@echo "  build - Builds the full executable"
	@echo "  run - Run the executable"
	@echo "  clean - Clean all build artifacts"

$(OUT): $(OBJFILES)
	$(CC) $(LDFLAGS) -o "$@" $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o "$@" -c $^

$(BUILD_DIR):
	mkdir -p "$@"
