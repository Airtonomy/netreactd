BUILD_DIR := _build
OUT := $(BUILD_DIR)/netreactd
CFLAGS := -O -Wall
CC := gcc
LDFLAGS :=
LDLIBS := -lpthread

CFILES := $(wildcard **.c)
OBJFILES := $(foreach file,$(CFILES),$(BUILD_DIR)/$(file:.c=.o))

PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
BINDIR = ${EXEC_PREFIX}/bin

.PHONY: all
all: $(OUT)

.PHONY: run
run: $(OUT)
	$(OUT)

.PHONY: install
install: $(OUT) $(BINDIR)
	install "$(OUT)" "$(BINDIR)"

.PHONY: uninstall
uninstall:
	rm -f "$(BINDIR)/$(notdir $(OUT))"

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: help
help:
	@echo 'Logical targets:'
	@echo '  all - Builds the full executable (DEFAULT)'
	@echo '  run - Run the executable'
	@echo '  install - Install the executable to $$EXEC_PREFIX'
	@echo '  clean - Clean all build artifacts'

$(OUT): $(OBJFILES)
	$(CC) $(LDFLAGS) -o "$@" $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o "$@" -c $^ $(LDLIBS)

$(BUILD_DIR):
	mkdir -p "$@"

$(BINDIR):
	mkdir -p "$@"
