TARGET = mysh format test
MYSH = mysh.o sighand.o job.o builtin.o

all: $(TARGET)

test: src/fs/test.c src/fs/fs.h
	gcc -o $@ src/fs/test.c

format: src/fs/format.c src/fs/disk.h
	gcc -o $@ src/fs/format.c

mysh: $(MYSH)
	gcc -o $@ $(MYSH)
mysh.o: src/shell/mysh.c sighand.o job.o builtin.o
	gcc -c -o $@ src/shell/mysh.c
sighand.o: src/shell/sighand.c src/shell/sighand.h
	gcc -c -o $@ src/shell/sighand.c
job.o: src/shell/job.c src/shell/job.h
	gcc -c -o $@ src/shell/job.c
builtin.o: src/shell/builtin.c src/shell/builtin.h
	gcc -c -o $@ src/shell/builtin.c

clean:
	rm -f $(TARGET) $(MYSH) *.a *.o *~ vgcore*
