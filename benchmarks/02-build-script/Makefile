# Build script in Make
# Build a C project with dependencies
# Lines: 48 | Complexity: High (tabs matter, shell escaping, implicit rules)

# Variables
CC = gcc
CFLAGS = -Wall -O2 -std=c99
LDFLAGS = -lm
SRCDIR = src
OBJDIR = obj
BINDIR = bin
TARGET = $(BINDIR)/app

# Find sources
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

# Phony targets
.PHONY: all clean test install

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

# Compile
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Directories (order-only prerequisites)
$(OBJDIR):
	mkdir -p $@

$(BINDIR):
	mkdir -p $@

# Clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Test (depends on build)
test: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET) --test || { echo "Tests failed!"; exit 1; }
	@echo "All tests passed!"

# Install
INSTALL_DIR = /usr/local/bin
install: $(TARGET)
	@echo "Installing to $(INSTALL_DIR)"
	@cp $(TARGET) $(INSTALL_DIR)/
	@echo "Installed: $(INSTALL_DIR)/app"

# Debug build
debug: CFLAGS = -Wall -g -O0 -DDEBUG
debug: $(TARGET)

# Check if sources changed
$(OBJDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/config.h
$(OBJDIR)/utils.o: $(SRCDIR)/utils.c $(SRCDIR)/utils.h
$(OBJDIR)/parser.o: $(SRCDIR)/parser.c $(SRCDIR)/parser.h $(SRCDIR)/config.h
