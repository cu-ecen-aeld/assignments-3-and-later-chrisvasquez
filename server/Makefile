# A default target which builds the aesdsocket application

# A clean target which removes the aesdsocket application and all .o files

# Support for cross-compilation.  
# You should be able to generate an application for the native build platform 
# when GNU make variable CROSS_COMPILE is not specified on the make command line.  
# When CROSS_COMPILE is specified with aarch64-none-linux-gnu- 
# (note the trailing -)your makefile should compile successfully using the 
# cross compiler installed in step 1.

CC = g++

ifeq (${CROSS_COMPILE}, aarch64-none-linux-gnu-)
	CC = aarch64-none-linux-gnu-gcc
endif

all: aesdsocket

writer:
	${CC} aesdsocket.c -o aesdsocket

clean:
	rm -rf aesdsocket
	rm -rf *.o

.PHONY: clean