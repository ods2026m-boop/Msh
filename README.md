# Msh — Monkey Shell

Msh is a POSIX-style shell written in C99, targeting musl libc with no external
runtime dependencies (no readline, ncurses, or libedit). It is part of the ODS-OS
project: an independent, from-scratch Linux distribution.

## POSIX Compliance

Msh follows POSIX.1-2008 (IEEE Std 1003.1, 2017 Edition) for its system-call
surface.  Only the following headers and syscalls are used:

```
Headers:  <stdio.h> <stdlib.h> <string.h> <unistd.h> <fcntl.h>
          <sys/wait.h> <sys/stat.h> <signal.h> <errno.h> <stdarg.h>
          <termios.h> <wordexp.h>

Syscalls: fork, execvp, waitpid, pipe, dup2, chdir, getcwd,
          signal, kill, read, write, open, close, getpid, getppid,
          isatty, tcsetpgrp, tcgetpgrp.
```

## Build

```sh
make
```

This uses `gcc -std=c99 -Wall -Wextra -Werror`.  The same source tree also
builds cleanly with `musl-gcc`.

## Run

```sh
./msh            # interactive shell
./msh -c 'echo hi'   # run a single command
```

## Debug Mode

Debug output is **off by default**.  Enable it with either:

```sh
./msh -x            # CLI flag
./msh --debug       # long flag
MSH_DEBUG=1 ./msh   # environment variable
```

When enabled, the lexer and main loop print internal state to `stderr`.

## Usage Examples

### Quoted strings

```sh
echo "hello from msh"          # -> hello from msh
echo 'literal $X'              # -> literal $X  (single quotes suppress expansion)
```

### Arithmetic expansion

```sh
echo $((2+3))                  # -> 5
echo $((1+(2*3)))              # -> 7
```

### Command substitution

```sh
echo $(echo nested)            # -> nested
echo "$(date +%Y)"             # -> 2026 (example)
```

### Variable expansion in double quotes

```sh
X=world
echo "hello $X"                # -> hello world
```

### Aliases

```sh
alias ll="echo aliased"
ll                             # -> aliased
```

### Functions

```sh
myfunc() { echo "in function"; }
myfunc                         # -> in function
```

### Redirection

```sh
echo test > /tmp/file.txt
cat /tmp/file.txt              # -> test
```

### Pipelines

```sh
cat file | grep pattern
cat file | grep a | grep p
```

### Job control

```sh
sleep 5 &                     # run in background
jobs                          # list background jobs
```

## Tests

```sh
make test
```

This runs:

- **Unit tests** (C programs linked against the msh object files):
  - `tests/unit/test_lexer` — tokenization of quotes, arithmetic, command-sub
  - `tests/unit/test_expand` — variable, arithmetic, tilde, and quote expansion
  - `tests/unit/test_parser` — AST construction for commands, pipelines, function defs

- **Integration tests** (shell scripts invoking `./msh -c`):
  - `tests/integration/test_builtins.sh`
  - `tests/integration/test_pipeline.sh`
  - `tests/integration/test_redirection.sh`
  - `tests/integration/test_jobs.sh`

## License

See `LICENSE`.
