CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c99 -O2 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS =

SRC_DIR = src
INC_DIR = include
BIN     = msh

SRCS = src/main.c src/lexer.c src/parser.c src/exec.c src/util.c src/redir.c src/job.c \
        src/signal.c src/var.c src/expand.c src/history.c src/completion.c src/prompt.c \
        src/config.c src/builtin.c src/pipeline.c \
        src/builtins/cd.c src/builtins/echo.c src/builtins/exit.c src/builtins/pwd.c \
        src/builtins/export.c src/builtins/unset.c src/builtins/alias.c src/builtins/history.c \
        src/builtins/jobs.c src/builtins/fg.c src/builtins/bg.c src/builtins/type.c \
        src/builtins/which.c src/builtins/source.c src/builtins/kill.c src/builtins/umask.c src/builtins/shopt.c src/builtins/test.c src/builtins/if_while_for.c src/builtins/function.c
SRCS += src/line_edit.c src/options.c src/compound.c src/array.c src/function.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -c -o $@ $<

TESTS = tests/unit/test_lexer tests/unit/test_expand tests/unit/test_parser

MSH_OBJS = src/main.o src/lexer.o src/parser.o src/exec.o src/util.o src/redir.o src/job.o \
           src/signal.o src/var.o src/expand.o src/history.o src/completion.o src/prompt.o \
           src/config.o src/builtin.o src/pipeline.o \
           src/builtins/cd.o src/builtins/echo.o src/builtins/exit.o src/builtins/pwd.o \
           src/builtins/export.o src/builtins/unset.o src/builtins/alias.o src/builtins/history.o \
           src/builtins/jobs.o src/builtins/fg.o src/builtins/bg.o src/builtins/type.o \
           src/builtins/which.o src/builtins/source.o src/builtins/kill.o src/builtins/umask.o \
           src/builtins/shopt.o src/builtins/test.o src/builtins/if_while_for.o \
           src/builtins/function.o src/line_edit.o src/options.o src/compound.o src/array.o \
           src/function.o

test: $(BIN) $(TESTS)
	@echo "Running unit tests..."
	@for t in $(TESTS); do echo "  $$t"; "$$t" || exit 1; done
	@echo "Running integration tests..."
	@bash tests/integration/test_builtins.sh || exit 1
	@bash tests/integration/test_pipeline.sh || exit 1
	@bash tests/integration/test_redirection.sh || exit 1
	@bash tests/integration/test_jobs.sh || exit 1
	@echo "All tests passed"

tests/unit/test_lexer: tests/unit/test_lexer.c $(MSH_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $< $(filter-out src/main.o,$(MSH_OBJS)) $(LDFLAGS)

tests/unit/test_expand: tests/unit/test_expand.c $(MSH_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $< $(filter-out src/main.o,$(MSH_OBJS)) $(LDFLAGS)

tests/unit/test_parser: tests/unit/test_parser.c $(MSH_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $< $(filter-out src/main.o,$(MSH_OBJS)) $(LDFLAGS)

clean:
	rm -f $(OBJS) $(BIN) $(TESTS)