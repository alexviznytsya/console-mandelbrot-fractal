build:
	g++ mandelbrot.cpp -o mandelbrot -std=c++11 -Wall
	g++ mandelCalc.cpp -o mandelCalc -std=c++11 -Wall
	g++ mandelDisplay.cpp -o mandelDisplay -std=c++11 -Wall

clean:
	rm mandelbrot mandelCalc mandelDisplay

run:
	clear
	./mandelbrot
