TARGET = mysh format test
MYSH = mysh.o sighand.o job.o builtin.o
FS = fs.o

all: $(TARGET)

test: src/fs/test.c src/fs/fs.h src/fs/fs.c
	gcc -g -o $@ src/fs/test.c src/fs/fs.h src/fs/fs.c
format: src/fs/format.c src/fs/disk.h
	gcc -g -o $@ src/fs/format.c
mysh: $(MYSH) $(FS)
	gcc -g -o $@ $(MYSH) $(FS)

fs.o: src/fs/fs.c src/fs/fs.h src/fs/disk.h src/fs/error.h
	gcc -c -g -o $@ src/fs/fs.c
mysh.o: src/shell/mysh.c sighand.o job.o builtin.o
	gcc -c -g -o $@ src/shell/mysh.c
sighand.o: src/shell/sighand.c src/shell/sighand.h src/shell/job.h
	gcc -c -g -o $@ src/shell/sighand.c
job.o: src/shell/job.c src/shell/job.h
	gcc -c -g -o $@ src/shell/job.c
builtin.o: src/shell/builtin.c src/shell/builtin.h src/shell/job.h src/shell/mysh.h \
src/fs/fs.h src/fs/disk.h src/fs/error.h
	gcc -c -g -o $@ src/shell/builtin.c

clean:
	rm -f $(TARGET) $(MYSH) $(FS) *.a *.o *~ vgcore*
