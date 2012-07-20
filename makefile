CPP = g++ -g
JAVA = javac

PKGCONFIG = 'pkg-config --cflags --libs playerc++'

all: RFIDdriver RFIDdriver.so

clean:
	rm -f main *.o *.so

RFIDdriver.so: RFIDdriver.o
	$(CPP) $(LIBS) -shared -nostartfiles -o $@ $^ $(PKGCONFIG)

RFIDdriver: RFIDdriver.h RFIDdriver.cpp
	$(CPP) $(LIBS) -c RFIDdriver.cpp $(PKGCONFIG)
