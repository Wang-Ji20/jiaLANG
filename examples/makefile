SRCS := $(shell find -name '*.jia')
OBJS := $(SRCS:%=%.show)
OUTS := $(SRCS:%=%.out)

all: display output

display: $(OBJS)
	echo shown $^

output: $(OUTS)
	echo out $^

%.jia.show: %.jia
	../build/main $^

%.jia.out: %.jia
	../build/main $^ > $^.json

clean:
	rm -f *.exe
	rm -f *.out