
EXE=fblauncher
SRC=	main.c 

${EXE}:	${SRC}
	gcc -I.. ${CFLAGS} ${SRC} -lm ../libfbui.a -g -o ${EXE}
	strip ${EXE}

clean:
	rm -f ${EXE}

install: ${EXE}
	cp ${EXE} /usr/bin
	chmod 755 /usr/bin/${EXE}

