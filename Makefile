# See UNLICENSE file for copyright and license details.

include config.mk

# Variables

DEP_DIR = dep
OBJ_DIR = obj
OUT_DIR = bin

BIN = nokia-keypad
OUT = ${OUT_DIR}/${BIN}
SRC = ${BIN}.c
DEP = $(wildcard ${DEP_DIR}/*.c)
OBJ = $(patsubst %.c, ${OBJ_DIR}/%.o, ${SRC}) $(patsubst ${DEP_DIR}/%.c, ${OBJ_DIR}/%.o, ${DEP})

# Targets

all: ${OUT}

release: EXTRAFLAGS = -O2 -DNDEBUG
release: clean
release: ${OUT}

${OUT}: ${OUT_DIR} ${OBJ_DIR} ${OBJ}
	${CC} ${CFLAGS} ${OBJ} -o $@ ${LDFLAGS}

${OUT_DIR}:
	mkdir $@

${OBJ_DIR}:
	mkdir $@

${OBJ_DIR}/%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

${OBJ_DIR}/%.o: ${DEP_DIR}/%.c
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -rf ${OBJ_DIR} ${OUT_DIR}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${OUT} ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < ${BIN}.1 > ${DESTDIR}${MANPREFIX}/man1/${BIN}.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/${BIN}.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/${BIN}\
		${DESTDIR}${MANPREFIX}/man1/${BIN}.1

.PHONY: all clean release install uninstall
