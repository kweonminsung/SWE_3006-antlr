# makefile for B2C in C++
PROG=b2c
G4=B.g4

ANTLR4=java -jar /usr/local/lib/antlr-complete.jar -Dlanguage=Cpp -no-listener -visitor -o antlr4-cpp
ANTLR4_RUNTIME=/usr/local/include/antlr4-runtime
CFLAGS=-I. -I$(ANTLR4_RUNTIME)
LIBS=-lantlr4-runtime

INPUT_B=input.b
OUTPUT_C=output.c
WFLAGS=-Wconversion -Wall -pedantic


ANTLR4DIR=antlr4-cpp
OBJ=B2CMain.o $(ANTLR4DIR)/BBaseVisitor.o $(ANTLR4DIR)/BLexer.o $(ANTLR4DIR)/BVisitor.o $(ANTLR4DIR)/BParser.o

default: antlr $(PROG)
	
$(PROG): $(OBJ)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

%.o:	%.cpp
	$(CXX) -c -o $@ $< $(CFLAGS)

antlr: $(G4)
	$(ANTLR4) $<

run: antlr $(PROG)
	./$(PROG) $(INPUT_B) | indent > $(OUTPUT_C)
	$(CXX) $(WFLAGS) $(OUTPUT_C)

.PHONY: clean

clean:
	rm -f $(PROG) *.o $(ANTLR4DIR)/*.o *~ core a.out

wipe:	clean
	rm -rf $(ANTLR4DIR)
	
