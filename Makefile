PROJECT = theft
OPTIMIZE = -O3
WARN = -Wall -Wextra -pedantic
#CDEFS += 
CFLAGS += -std=c99 -g ${WARN} ${CDEFS} ${OPTIMIZE}
#LDFLAGS +=

# A tautological compare is expected in the test suite.
CFLAGS += -Wno-tautological-compare -Wno-type-limits

all: lib${PROJECT}.a
all: test_${PROJECT}

TEST_CFLAGS=	${CFLAGS} -Wno-tautological-compare
TEST_LDFLAGS=	${LDFLAGS}

OBJS= theft.o theft_bloom.o theft_hash.o theft_mt.o

TEST_OBJS=	test_theft.o \
		test_theft_prng.o \

lib${PROJECT}.a: ${OBJS}
	ar -rcs lib${PROJECT}.a ${OBJS}

test_${PROJECT}: ${OBJS} ${TEST_OBJS}
	${CC} -o $@ ${OBJS} ${TEST_OBJS} ${TEST_CFLAGS} ${TEST_LDFLAGS}

test: ./test_${PROJECT}
	./test_${PROJECT}

clean:
	rm -f ${PROJECT} test_${PROJECT} *.o *.a *.core


# Installation
PREFIX ?=/usr/local
INSTALL ?= install
RM ?=rm

install: lib${PROJECT}.a
	${INSTALL} -d ${PREFIX}/lib/
	${INSTALL} -c lib${PROJECT}.a ${PREFIX}/lib/lib${PROJECT}.a
	${INSTALL} -d ${PREFIX}/include/
	${INSTALL} -c ${PROJECT}.h ${PREFIX}/include/
	${INSTALL} -c ${PROJECT}_types.h ${PREFIX}/include/

uninstall:
	${RM} -f ${PREFIX}/lib/lib${PROJECT}.a
	${RM} -f ${PREFIX}/include/${PROJECT}.h
	${RM} -f ${PREFIX}/include/${PROJECT}_types.h


# Other dependencies
theft.o: Makefile
theft.o: theft.h theft_types.h theft_types_internal.h
