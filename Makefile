CC = $(LLVM_TOOLCHAIN)/clang
CFLAGS = -O2 -shared -fPIC -lgraalvm-llvm

target = libcallbench.so

all: $(target)

$(target): core.c Makefile
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(target)

.PHONY: all clean
