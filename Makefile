CXX=gcc

all: swiper

swiper:	swiper.o mymath.o
	$(CXX) swiper.o mymath.o -o swiper -lm

swiper.o: swiper.c swiper.h
	$(CXX) -c swiper.c

mymath.o: mymath.c mymath.h
	$(CXX) -c mymath.c

clean:
	rm -f *.o swiper
