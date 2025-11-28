CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -pthread
DEBUGFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -pthread -DDEBUG
LDFLAGS := -pthread

SRCDIR := src
INCDIR := include
BUILDDIR := build
BINDIR := bin
TESTDIR := tests

TARGET := $(BINDIR)/cpp-network-server

SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

INCLUDES := -I$(INCDIR)

.PHONY: all
all: $(TARGET)
	@echo "Build complete"

$(BUILDDIR) $(BINDIR):
	@mkdir -p $@

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

.PHONY: debug
debug: CXXFLAGS := $(DEBUGFLAGS)
debug: clean all
	@echo "Debug build complete"

.PHONY: clean
clean:
	@rm -rf $(BUILDDIR) $(BINDIR)
	@echo "Clean complete"

.PHONY: run
run: all
	@echo "Starting server..."
	@$(TARGET)

.PHONY: test
test: all test-server
	@echo "Test client built. Run './bin/test-server' to test the server"

test-server: $(TESTDIR)/test_server.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $< -o $(BINDIR)/test-server

.PHONY: help
help:
	@echo "$(BLUE)Available targets:$(NC)"
	@echo "  all          - Build the server (default)"
	@echo "  debug        - Build with debug symbols"
	@echo "  clean        - Remove build files"
	@echo "  run          - Build and run the server"
	@echo "  test         - Build test client"
	@echo "  help         - Show this help message"

-include $(DEPS)

# Print variables (for debugging Makefile)
.PHONY: print-%
print-%:
	@echo '$*=$($*)'
