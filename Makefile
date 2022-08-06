CFLAGS= -Wall -O3 -g -Wall -Wextra
LDFLAGS= 
NAME=main
OUTFILE=mandelbrot
CC=gcc

all: ${NAME}

${NAME}: ${NAME}.c
	${CC} ${NAME}.c -o ${OUTFILE} ${CFLAGS} ${LDFLAGS}

run: ${OUTFILE}
	./${OUTFILE}
