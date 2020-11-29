# Dirty COW Exploit

## About

This project contains an implementation of the [Dirty COW exploit](https://www.cs.toronto.edu/~arnold/427/18s/427_18S/indepth/dirty-cow/index.html). There are also examples of how the exploit can be used, which are designed to run on a 32-bit image of Ubuntu 14.04. The ISO used to develop these can be found [here](http://old-releases.ubuntu.com/releases/14.04.0/ubuntu-14.04.1-desktop-i386.iso).

## Compilation

From the project root, run `make -C app`. This places the executable `dirtycow` in the `app` directory.

## Usage

Assuming the executable is in the current working directory:

```
./dirtycow [-f FILE | -s STRING | -x HEX] [-o OFFSET | -a] TARGET_FILE
```

The executable writes a payload to the file `TARGET_FILE`, which must be a pre-existing readable file. The original contents of the file are overwritten with the payload. However, if the payload is shorter than the file's contents, the contents are not truncated.

The payload can be specified in one of three ways: from a file, as a string of characters, or as a string of hexadecimal bytes. These correspond to the command-line flags `-f`, `-s`, and `-x`, respectively. If none of these are chosen, the payload is taken from standard input instead.

The default behavior of the program is to write the payload at the beginning of the target file. This behavior can be overriden with the `-o OFFSET` flag to start writing the payload at the given byte offset, or the `-a` flag to append the payload to the target file. (TODO: Clarify whether `OFFSET` is in decimal, hex, etc.)

## Scripts

TODO: Describe the scripts
