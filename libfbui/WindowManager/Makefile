
EXE=fbwm
SRC=	main.c jpeg.c rect.c

test:	${SRC}
	gcc -lm -ljpeg -I.. -g ${SRC} ../libfbui.a -o ${EXE} 
	# strip ${EXE}

clean:
	rm -f ${EXE}

install: ${EXE}
	cp ${EXE} /usr/bin
	chmod 755 /usr/bin/${EXE}

