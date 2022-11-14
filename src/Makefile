# This Makefile is designed to be simple and readable.  It does not
# aim at portability.  It requires GNU Make.

BASE = main
BISON = bison
CXX = clang++
FLEX = flex
XSLTPROC = xsltproc
CXXFLAGS = `llvm-config --cxxflags` -DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1

all: clean $(BASE)

%.cc %.hh %.xml %.gv: %.yy
	$(BISON) $(BISONFLAGS) --xml --graph=$*.gv -o $*.cc $<

%.cc: %.ll
	$(FLEX) $(FLEXFLAGS) -o$@ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o$@ $<

$(BASE): $(BASE).o  AST.o CodegenVisitor.o PrintVisitor.o driver.o parser.o scanner.o 
	$(CXX) `llvm-config --cxxflags --ldflags --libs core` -o $@ $^

$(BASE).o: parser.hh
parser.o: parser.hh
scanner.o: parser.hh



html: parser.html
%.html: %.xml
	$(XSLTPROC) $(XSLTPROCFLAGS) -o $@ $$($(BISON) --print-datadir)/xslt/xml2xhtml.xsl $<

CLEANFILES =										\
  $(BASE) *.o										\
  parser.hh parser.cc parser.output parser.xml parser.html parser.gv location.hh	\
  scanner.cc *.out *.exe
clean:
	rm -f $(CLEANFILES)
