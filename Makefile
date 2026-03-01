.SUFFIXES:

APP_TITLE           := M3DS Model Converter
APP_DESCRIPTION     := A tool to convert common 3D model formats to .mod3ds
APP_AUTHOR          := HyperDir

APP_VER_MAJOR       := 1
APP_VER_MINOR       := 0
APP_VER_PATCH       := 0


C_VERSION		    := 23
CXX_VERSION		    := 26


BUILD_DIR           := build
SOURCE_DIR         := source
INCLUDE_DIR         := include
EMBED_DIR           := $(BUILD_DIR)

OUTPUT_DIR          := output

# Compilation Flags
WARNINGS            := -Wall -Werror -Wextra -Wconversion -Wpedantic
MAKEFLAGS           := -j14 --silent
COMMON_FLAGS        := $(WARNINGS) -O2
C_FLAGS             := $(COMMON_FLAGS) -std=c$(C_VERSION)
CXX_FLAGS           := $(COMMON_FLAGS) -std=c++$(CXX_VERSION)

LD_FLAGS            :=

LIBS                :=
LIB_DIRS            :=

# Build Variable Setup
recurse              = $(shell find $2 -type $1 -name '$3' 2> /dev/null)

C_FILES             := $(foreach dir,$(SOURCE_DIR),$(call recurse,f,$(dir),*.c))
CXX_FILES           := $(foreach dir,$(SOURCE_DIR),$(call recurse,f,$(dir),*.cpp))

O_FILES             := $(patsubst $(SOURCE_DIR)/%,$(BUILD_DIR)/%,$(addsuffix .o, $(basename $(C_FILES) $(CXX_FILES))))

INCLUDE             := $(foreach dir,$(INCLUDE_DIR),-I./$(dir)) $(foreach dir,$(LIB_DIRS),-isystem $(dir)/include)
LIB_PATHS           := $(foreach dir,$(LIB_DIRS),-L$(dir)/lib)


VERSION_FLAGS       := -DAPP_VERSION_MAJOR=$(APP_VER_MAJOR) -DAPP_VERSION_MINOR=$(APP_VER_MINOR) -DAPP_VERSION_PATCH=$(APP_VER_PATCH)

SPACE               := $() $()
OUTPUT_NAME         := $(subst $(SPACE),,$(APP_TITLE))
OUTPUT_FILE         := $(OUTPUT_DIR)/$(OUTPUT_NAME)

# Rules
.PHONY: all clean directories

all: directories $(OUTPUT_FILE)

$(OUTPUT_FILE): $(O_FILES)
	@echo "Linking: $@"
	@g++ $(O_FILES) $(LIB_PATHS) $(LIBS) $(LD_FLAGS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(info Compiling $<...)
	@gcc $(C_FLAGS) $(VERSION_FLAGS) $(INCLUDE) -c $< -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(info Compiling $<...)
	@g++ $(CXX_FLAGS) $(VERSION_FLAGS) $(INCLUDE) -c $< -o $@

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OUTPUT_DIR)

clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_DIR) $(OUTPUT_DIR)