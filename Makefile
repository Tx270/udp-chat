CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
BUILD_DIR = build

# ---- Platform detection ----
ifeq ($(OS),Windows_NT)
    PLATFORM = windows
    OTHER    = linux
    EXE      = .exe
    LDLIBS   = -lws2_32 -liphlpapi

    # MSYS2 / Git Bash / Cygwin provide a POSIX shell; plain cmd.exe doesn't
    ifneq (,$(findstring sh,$(notdir $(SHELL))))
        MKDIR = mkdir -p $(BUILD_DIR)
        RM    = rm -f
        RUN   = ./$(TARGET)
    else
        MKDIR = if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
        RM    = del /Q /F
        RUN   = $(subst /,\,$(TARGET))
    endif
else
    PLATFORM = linux
    OTHER    = windows
    EXE      =
    LDLIBS   =
    MKDIR    = mkdir -p $(BUILD_DIR)
    RM       = rm -f
    RUN      = ./$(TARGET)
endif

TARGET = $(BUILD_DIR)/udp-chat$(EXE)

# ---- Source discovery (portable recursive wildcard, no `find`) ----
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Everything under src, minus the other platform's folder
SOURCES = $(filter-out src/$(OTHER)/%,$(call rwildcard,src,*.c))
HEADERS = $(filter-out src/$(OTHER)/%,$(call rwildcard,src,*.h))

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc $(SOURCES) -o $(TARGET) $(LDLIBS)

$(BUILD_DIR):
	$(MKDIR)

run: $(TARGET)
	$(RUN)

clean:
ifeq ($(findstring del,$(RM)),del)
	-$(RM) $(subst /,\,$(TARGET))
else
	$(RM) $(TARGET)
endif

.PHONY: all run clean