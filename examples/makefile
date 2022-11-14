SRCS := $(shell find -name '*.jia')
OBJS := $(SRCS:%=%.exe)

all: $(OBJS)
	echo built $^

%.jia.exe: %.jia
	../build/main $^ && $(CXX) output.out chelper.cc -o $@

clean:
	rm *.exe
	rm *.out