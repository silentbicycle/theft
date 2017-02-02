PROJECT = 	theft
BUILD =		build
INC =		inc
VENDOR =	vendor
OPTIMIZE = 	-O3
WARN = 		-Wall -Wextra -pedantic
CDEFS +=
CINCS += 	-I${INC} -I${VENDOR}
CFLAGS += 	-std=c99 -g ${WARN} ${CDEFS} ${OPTIMIZE} ${CINCS}
LDFLAGS +=

all: ${BUILD}/lib${PROJECT}.a
all: ${BUILD}/test_${PROJECT}

# A tautological compare is expected in the test suite.
TEST_CFLAGS += 	${CFLAGS} -Wno-tautological-compare -Wno-type-limits
TEST_LDFLAGS +=	${LDFLAGS}

OBJS= 		${BUILD}/theft.o \
		${BUILD}/theft_bloom.o \
		${BUILD}/theft_call.o \
		${BUILD}/theft_hash.o \
		${BUILD}/theft_mt.o \
		${BUILD}/theft_random.o \
		${BUILD}/theft_run.o \
		${BUILD}/theft_shrink.o \
		${BUILD}/theft_trial.o \

TEST_OBJS=	${BUILD}/test_theft.o \
		${BUILD}/test_theft_prng.o \


# Basic targets

test: ${BUILD}/test_${PROJECT}
	${BUILD}/test_${PROJECT}

clean:
	rm -rf ${BUILD}

tags: ${BUILD}/TAGS

${BUILD}/lib${PROJECT}.a: ${OBJS}
	ar -rcs ${BUILD}/lib${PROJECT}.a ${OBJS}

${BUILD}/test_${PROJECT}: ${OBJS} ${TEST_OBJS}
	${CC} -o $@ ${OBJS} ${TEST_OBJS} ${TEST_CFLAGS} ${TEST_LDFLAGS}

${BUILD}/%.o: src/%.c ${BUILD}
	${CC} -c -o $@ ${CFLAGS} $<

${BUILD}/%.o: test/%.c ${BUILD}
	${CC} -c -o $@ ${TEST_CFLAGS} $<

${BUILD}/TAGS: ${BUILD}
	etags -o $@ *.[ch]

${BUILD}:
	mkdir ${BUILD}

# Installation
PREFIX ?=	/usr/local
INSTALL ?= 	install
RM ?=		rm

install: ${BUILD}/lib${PROJECT}.a
	${INSTALL} -d ${PREFIX}/lib/
	${INSTALL} -c ${BUILD}/lib${PROJECT}.a ${PREFIX}/lib/lib${PROJECT}.a
	${INSTALL} -d ${PREFIX}/include/
	${INSTALL} -c ${INC}/${PROJECT}.h ${PREFIX}/include/
	${INSTALL} -c ${INC}/${PROJECT}_types.h ${PREFIX}/include/

uninstall:
	${RM} -f ${PREFIX}/lib/lib${PROJECT}.a
	${RM} -f ${PREFIX}/include/${PROJECT}.h
	${RM} -f ${PREFIX}/include/${PROJECT}_types.h


# Other dependencies
${BUILD}/theft.o: Makefile ${INC}/*.h
