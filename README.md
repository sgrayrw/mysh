# mysh

A powerfull shell with full job control and a unix-like inode-based underlying file system.

Authors: [Yuxiao Wang](https://github.com/sgrayrw), [Ruikang Shi](https://github.com/Shadoom7), [Jiyu Huang](https://github.com/JiyuHuang)

## Usage

- clone the repo
- run `make` to build the executables
- run `./format DISK` to make a disk for the file system that the shell runs on
- run `./mysh` to launch the shell
- login credentials
  - regular user: name `John_Doe`, passwd `000000`
  - super user: name `Admin`, passwd `666666`

## List of supported commands

- all binaries in `$PATH` (e.g. `echo`, `ps`, etc.)
- job control
  - `fg`, `bg`, `jobs`, `&`, `kill`
- file system
  - file: `cat`, `more`, `>` and `<` (redirection), `rm`
  - directory: `ls [-l]`, `mkdir`, `rmdir`, `cd`, `pwd`
  - disk: `mount`, `umount`
  - permission control: `chmod`
