cmake_minimum_required(VERSION 3.14)
project(fs C)

set(CMAKE_C_STANDARD 99)

add_executable(fs src/shell/mysh.c src/shell/mysh.h src/shell/job.c src/shell/job.h src/shell/builtin.c src/shell/builtin.h src/shell/sighand.c src/shell/sighand.h src/fs/disk.h src/fs/fs.h src/fs/format.c src/fs/fs.c src/fs/test.c src/fs/error.h)