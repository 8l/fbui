
EXE=fbload
SRC=	main.c 

${EXE}:	${SRC}
	gcc -I.. -g ${SRC} ../libfbui.a -lm -o ${EXE}
	strip ${EXE}

clean:
	rm -f ${EXE}

install: ${EXE}
	cp ${EXE} /usr/bin
	chmod 755 /usr/bin/${EXE}

