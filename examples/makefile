SRCS := $(shell find -name '*.jia')
OBJS := $(SRCS:%=%.show)

all: $(OBJS)
	echo shown $^

%.jia.show: %.jia
	../build/main $^

clean:
	rm -f *.exe
	rm -f *.out