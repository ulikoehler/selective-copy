# selective-copy

Did you every try to copy a huge project file folder over the network even though you needed only a couple of files?
And then you wasted hours trying to find out experimentally what files you actually *need*

*selective-copy* is the solution for all those problem. It copies only what files are actually needed 

## How to install

First, install a somewhat recent version of Python 3 to run the python part.

In order to compile the native library, you need a C++ compiler like G++ and 

On Ubuntu you can install that using

```sh
sudo apt install cmake build-essential
```

Now compile *libopenlog* (see below for details on what it does):

```sh
cmake .
make
```

This will compile `libopenlog.so`.

## How to run

*TL;DR:*
```sh
./selective-copy.py make --workdir ~/myprojectdir --copy-to /tmp/myproject-copy
```

This will run `make` in `~/myprojectdir` and copy every file read by `make` (and all its child processes) from `myprojectdir` to `/tmp/myproject-copy`. Files read from outside `~/myprojectdir` will not be copied. Files that are only written (and not opened for reading or read/write) are not copied.

Note that for *selective-copy* to work you need to do a clean build in your project using `make`, i.e. do `make clean` or equivalent before running *selective-copy*. If `make` or similar commands are not executed in a clean environment, they might not `open()` all relevant source files.

## How does it work?

*selective-copy* includes *libopenlog* which is a library that hacks into the `open()` syscall by using the *`LD_PRELOAD` trick*.

`selective-copy.py` will automatically run the command (e.g. `make`) with the correct environment variables like `LD_PRELOAD` and `LIBOPENLOG_PREFIX`. Since not only the command itself (e.g. `make`) will open files but also their child processes (like `g++`), each instance of `libopenlog` (i.e. each process running within `selective-copy`) will connect to `localhost:13485` (for read-opened files) and `localhost:13486` (for write-opened files)

The python script will collect all the read-opened absolute filenames using an `asyncio`-based server that easily allows to handle an arbitrary number of connections efficiently.

`libopenlog` will only send the server the filenames for files within the `LIBOPENLOG_PREFIX` directory. When using `selective-copy.py`, this defaults to `--workdir`, but you can override that using `--prefix`.
The purpose of that filter is to avoid copying files e.g. from `/usr/include`. If you want to copy *all* files, use `--prefix /`, i.e. prefix = root directory.

After the command has finished, the python script will compute the path of each file relative to `LIBOPENLOG_PREFIX` and then copy it inside the destination directory (i.e. `--copy-to`) using the same folder structure.

### Limitations

*selective-copy* will only copy files that are `open()`ed for reading, and will not work in the following circumstances:
* Empty directories will not be copied
* Some file attributes like ACLs and username/group will not be copied
* If the application depends on certain files or directories being present, but it never opens these files or directories, it might fail

### Credits

Inspired by [libeatmydata](https://github.com/stewartsmith/libeatmydata/) by [Steward Smith](https://github.com/stewartsmith/).