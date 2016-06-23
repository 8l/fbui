
EXE=fbdump
SRC=	 main.c jpeg.c

${EXE}:	${SRC}
	gcc -lm -I.. -g ${SRC} -o ${EXE} -ljpeg
	strip ${EXE}

clean:
	rm -f ${EXE}

install: ${EXE}
	cp ${EXE} /usr/bin
	chmod 755 /usr/bin/${EXE}
