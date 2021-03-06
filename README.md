# Dirty COW Exploit

## About

This project contains an implementation of the [Dirty COW exploit](https://www.cs.toronto.edu/~arnold/427/18s/427_18S/indepth/dirty-cow/index.html). There are also examples of how the exploit can be used, which are designed to run on a 32-bit image of Ubuntu 14.04. The ISO used to develop these can be found [here](http://old-releases.ubuntu.com/releases/14.04.0/ubuntu-14.04.1-desktop-i386.iso).

## Compilation

From the project root, run `make -C app`. This places the executable `dirtycow` in the `app` directory.

## Usage

Assuming the executable is in the current working directory:

```
./dirtycow [-h] [-f FILE | -s STRING] [-o OFFSET] TARGET_FILE
```

The executable writes a payload to the file `TARGET_FILE`, which must be a pre-existing readable file. The original contents of the file are overwritten with the payload. However, if the payload is shorter than the file's contents, the contents are not truncated.

The payload can be specified in one of two ways: from a file or as a string of characters. These correspond to the command-line flags `-f` and `-s`, respectively. If neither of these are chosen, the payload is taken from standard input instead.

The default behavior of the program is to write the payload at the beginning of the target file. This behavior can be overriden with the `-o OFFSET` flag to start writing the payload at the given byte offset. `OFFSET` can be decimal, octal, or hexadecimal.

## Scripts

There are two scripts that demonstrate usages of our exploit implementation:

1. `scripts/edit-passwd` can edit an arbitrary user in `/etc/passwd`, replacing their UID and GUID with root's UID and GUID, respectively. It also changes the user's shell to `/bin/sh` to avoid issues with users that cannot log in or have an unusual shell.
2. `scripts/edit-bashrc` edits `/etc/bash.bashrc` to attempt a reverse shell via a TCP connection to a given server and port whenever an interactive bash session is started.

Both scripts back up their respective `/etc` files to the user's home directory. This allows the scripts to be undone by overwriting the edited file with the back up.

The first script allows anyone with valid credentials on the vulnerable machine to escalate their privileges. It puts the system into an unstable state, however. Programs that check the user's UID or GUID, such as `sudo`, are unable to find the user, since their old UID is gone. As such, the script is best used to gain temporary root access (e.g., by logging into a different TTY), obtain permanent root access from there, and restore the edited user's UID and GUID.

In the second script, the shell gives a remote server a shell session. This session is logged into the user that spawned the bash process, and works for any user on the machine. Thus, if anyone runs `sudo bash` or similar, the server will have root access. Unlike the first exploit, this is not guaranteed to give root access to the attacker. However, once the payload is installed in `/etc/bash.bashrc`, the attacker can gain remote access to the machine, which may allow further exploitation if a root shell is not obtained.
