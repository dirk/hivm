CC = gcc
CFLAGS = -g -Wall -c -std=c99 -I.

LDFLAGS = -lpthread -L.

# http://stackoverflow.com/questions/7004702/how-can-i-create-a-makefile-for-c-projects-with-src-obj-and-bin-subdirectories
SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:.c=.o)

OUT = libhivem.a

# all: $(SOURCES) $(EXECUTABLE)
all: $(OBJECTS) $(OUT)

# $(EXECUTABLE): $(OBJECTS)
# 	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
# 	chmod +x $(EXECUTABLE)
# 	mkdir -p $(EXDIR)
# 	cp $(EXECUTABLE) $(EXDIR)/$(EXECUTABLE)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@


$(OUT): $(OBJECTS)
	ar rcs $(OUT) $(OBJECTS)


TESTDIR = test
TESTSOURCES = test/test.c
TESTOBJECTS = $(TESTSOURCES:.c=.o)
TESTTARGET = test/test

test: $(TESTTARGET)

$(TESTTARGET): $(TESTOBJECTS)
	$(CC) $(LDFLAGS) $(TESTOBJECTS) -o $@


clean:
	rm -rf $(SRCDIR)/*.o
	rm -rf $(TESTDIR)/*.o
	rm -f libhivem.a
