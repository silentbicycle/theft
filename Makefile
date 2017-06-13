PROJECT = 	theft
BUILD =		build
SRC =		src
TEST =		test
INC =		inc
SCRIPTS =	scripts
VENDOR =	vendor
COVERAGE =	-fprofile-arcs -ftest-coverage
PROFILE =	-pg
OPTIMIZE = 	-O3
#OPTIMIZE = 	-O0 ${COVERAGE}
#OPTIMIZE = 	-O0 ${PROFILE}

WARN = 		-Wall -Wextra -pedantic
CDEFS +=
CINCS += 	-I${INC} -I${VENDOR} -I${BUILD}
CFLAGS += 	-std=c99 -g ${WARN} ${CDEFS} ${OPTIMIZE} ${CINCS}

# Note: -lm is only needed if using built-in floating point generators
LDFLAGS +=	-lm

all: ${BUILD}/lib${PROJECT}.a
all: ${BUILD}/test_${PROJECT}

# A tautological compare is expected in the test suite.
TEST_CFLAGS += 	${CFLAGS} -I${SRC} -Wno-tautological-compare -Wno-type-limits
TEST_LDFLAGS +=	${LDFLAGS}

OBJS= 		${BUILD}/theft.o \
		${BUILD}/theft_autoshrink.o \
		${BUILD}/theft_bloom.o \
		${BUILD}/theft_call.o \
		${BUILD}/theft_hash.o \
		${BUILD}/theft_mt.o \
		${BUILD}/theft_random.o \
		${BUILD}/theft_run.o \
		${BUILD}/theft_shrink.o \
		${BUILD}/theft_trial.o \
		${BUILD}/theft_aux.o \
		${BUILD}/theft_aux_builtin.o \

TEST_OBJS=	${BUILD}/test_theft.o \
		${BUILD}/test_theft_autoshrink.o \
		${BUILD}/test_theft_autoshrink_ll.o \
		${BUILD}/test_theft_autoshrink_bulk.o \
		${BUILD}/test_theft_autoshrink_int_array.o \
		${BUILD}/test_theft_aux.o \
		${BUILD}/test_theft_error.o \
		${BUILD}/test_theft_prng.o \
		${BUILD}/test_theft_integration.o \


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

${BUILD}/%.o: ${SRC}/%.c ${SRC}/*.h ${INC}/* | ${BUILD}
	${CC} -c -o $@ ${CFLAGS} $<

${BUILD}/%.o: ${TEST}/%.c ${SRC}/*.h ${INC}/* | ${BUILD}
	${CC} -c -o $@ ${TEST_CFLAGS} $<

${BUILD}/TAGS: ${SRC}/*.c ${SRC}/*.h ${INC}/* | ${BUILD}
	etags -o $@ ${SRC}/*.[ch] ${INC}/*.h ${TEST}/*.[ch]

${SRC}/*.c: Makefile
${INT}/*.c: Makefile
${TEST}/*.c: Makefile


${BUILD}:
	mkdir ${BUILD}

${BUILD}/cover: | ${BUILD}
	mkdir ${BUILD}/cover

profile: test
	gprof build/test_theft

coverage: test | ${BUILD} ${BUILD}/cover
	ls -1 src/*.c | sed -e "s#src/#build/#" | xargs -n1 gcov
	@echo moving coverage files to ${BUILD}/cover
	mv *.gcov ${BUILD}/cover

${BUILD}/theft_autoshrink.o: ${BUILD}/bits_lut.h | ${BUILD}

${BUILD}/bits_lut.h: | ${BUILD}
	${SCRIPTS}/mk_bits_lut > $@

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

.PHONY: all test clean tags coverage profile install uninstall
