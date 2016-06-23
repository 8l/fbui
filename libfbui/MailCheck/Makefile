
EXE=fbcheck
SRC=	main.c check.c

test:	${SRC}
	gcc -lm -I.. -g ${SRC} ../libfbui.a -o ${EXE} 
	# strip ${EXE}

clean:
	rm -f ${EXE}

install: ${EXE}
	cp ${EXE} /usr/bin
	chmod 755 /usr/bin/${EXE}

