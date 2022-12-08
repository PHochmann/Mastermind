TARGET_EXEC  = Mastermind
BUILD_DIR    = ./bin/release
SRC_DIRS     = ./src
SRCS = $(shell find $(SRC_DIRS) -name *.c)
CFLAGS       = -MMD -MP -std=c99 -Wall -Wextra -Werror -pedantic -Werror=vla
LDFLAGS      = -lm -lreadline

# Compile with debugging flags if target is debug
ifneq (,$(filter $(MAKECMDGOALS),debug))
	BUILD_DIR    =  ./bin/debug
	INSTALL_PATH = .
	CFLAGS       += -DDEBUG -g3 -O0 -Wno-unused-variable -Wno-unused-parameter
endif

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

all: $(BUILD_DIR)/$(TARGET_EXEC)

debug: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo Done. Placed executable at $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $<
	@$(CC) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) -r ./bin

-include $(DEPS)
