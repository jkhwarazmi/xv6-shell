# xv6 Shell

## Project Description

A simple Unix-style shell written in C for the [xv6](https://github.com/mit-pdos/xv6-riscv) operating system. The shell reads commands from standard input and runs them with xv6's own system calls (`fork`, `exec`, `wait`, `pipe`, `dup`, `open`, `close` and `chdir`). It supports pipes, input/output redirection, command sequences and a built-in `cd`.

## Table of Contents
1. [Description](#project-description)
2. [Features](#features)
3. [Installation](#installation)
4. [Usage](#usage)
5. [How It Works](#how-it-works)
6. [Limitations](#limitations)

## Features

- **Command execution**: runs any xv6 user program with up to 10 arguments, separated by spaces or tabs
- **Pipes (`|`)**: connects the output of one command to the input of the next, and can be chained (e.g. `ls | grep test | cat`)
- **Input redirection (`<`)**: reads standard input from a file
- **Output redirection (`>`)**: writes standard output to a file, creating it or truncating it if it already exists
- **Sequences (`;`)**: runs commands one after another, waiting for each to finish before starting the next
- **Built-in `cd`**: changes the shell's working directory
- **Error handling**: rejects malformed input such as commands that start with `;`, `|` or `<`, redirections with no filename, lines over 100 characters and commands with too many arguments

These can be combined freely, with or without spaces around the operators:

```bash
>>> ls | grep test | cat > myoutput
>>> echo hello > input; cat < input > output; cat output
>>> cat README | grep github > g; cat < g | wc
>>> ls|grep test>f1;cat<f1>f2
>>> mkdir t; cd t; cd ..
```

## Installation

You need the RISC-V GNU toolchain and QEMU. The [xv6-riscv README](https://github.com/mit-pdos/xv6-riscv) explains how to install them.

1. Clone xv6:
```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
```

2. Copy the shell into xv6's user programs directory:
```bash
cp my_shell.c xv6-riscv/user/
```

3. Add the shell to the `UPROGS` list in `xv6-riscv/Makefile`:
```makefile
UPROGS=\
	$U/_cat\
	...
	$U/_my_shell\
```

4. Build and boot xv6 in QEMU:
```bash
cd xv6-riscv
make qemu
```

## Usage

At the xv6 prompt, start the shell:

```bash
$ my_shell
>>>
```

Type commands at the `>>>` prompt. To leave QEMU, press `Ctrl-A` and then `X`.

## How It Works

- `getcmd` prints the prompt and reads a line from standard input one character at a time into a 100-byte buffer. If a line is too long, the rest of it is discarded so that it isn't run as a separate command.
- `run_command` parses the buffer one character at a time. It splits arguments in place by replacing whitespace with null terminators, and it records any `;`, `|`, `<` or `>` it finds:
  - **`;`** ends the current command. The shell runs it in a child process, waits for the child to finish, then recursively runs the rest of the line.
  - **`|`** creates a pipe and forks twice. The left-hand command runs with its standard output connected to the pipe, and `run_command` is called recursively on the right-hand side with its standard input connected to the pipe.
  - **`<` / `>`** copy the next word as a filename. Before the command runs, the shell closes standard input or output and opens that file in its place.
- **`cd`** has to change the parent shell's directory, not the directory of the child that parsed the command. The child writes the path to a pipe shared with the parent and exits with status `2`. When the parent sees that status, it reads the path from the pipe and calls `chdir`.

## Limitations

- A command can have at most one `<` and one `>`
- No quoting, escaping, environment variables, globbing, append redirection (`>>`) or background jobs (`&`)
- Input lines are limited to 100 characters
- xv6 doesn't search a `PATH`, so a program is found relative to the current directory. After `cd`-ing into a subdirectory, run programs with a path, e.g. `../ls`
