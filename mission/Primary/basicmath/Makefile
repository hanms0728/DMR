FILE1 = basicmath_small.c   rad2deg.c  cubic.c   isqrt.c
FILE2 = basicmath_large.c   rad2deg.c  cubic.c   isqrt.c

all: basicmath_small basicmath_large

basicmath_small: ${FILE1} Makefile
	gcc -g -o2 -finstrument-functions ${FILE1} -o basicmath_small -lm
basicmath_large: ${FILE2} Makefile
	gcc -g -o2 -finstrument-functions ${FILE2} -o basicmath_large -lm

clean:
	rm -rf basicmath_small basicmath_large output*
