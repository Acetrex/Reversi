# Add the names of your .c files from your assignment here. Use a space to
# separate them. So something like:
#   SRCS = main.c potato.c
# Do not include .h files here
SRCS = given_driver.c

# This line is the name of the program you are building
BIN = proj3_driver

# Compiler flags to pass to the system's C compiler while building the source
# files. You can change the standard here to another C version by changing the
# -std flag
CFLAGS = -g -Wall -Wextra -pedantic -Wno-unused-parameter -Werror -std=gnu90

# Flags to pass to the linker while linking your program
# You shouldn't need to change this at all.
LDFLAGS = -g -lpthread

###################################################################
# You shouldn't need to touch anything below this line...         #
###################################################################
BUILD_DIR = build/make
OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SRCS)))

.PHONY: $(BIN)

$(BIN): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.PRECIOUS: $(BUILD_DIR)/. $(BUILD_DIR)%/.

$(BUILD_DIR)/.:
	mkdir -p $@

$(BUILD_DIR)%/.:
	mkdir -p $@

all: $(BIN)

clean:
	-rm -rf $(BUILD_DIR)
	-rm -f $(BIN)

.SECONDEXPANSION:

$(BUILD_DIR)/%.o: %.c | $$(@D)/.
	$(CC) -c $< -o $@ $(CFLAGS)
