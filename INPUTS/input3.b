#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

auto main() {
	auto i, j, k;
	auto a, b, c;
	auto f, g, h;
	auto x, y, z;

	i = 10; 
	j = 20;
	a = 'A'; 
	b = 'a';
	f = 3.14; 
	g = 2.718;
	x = true;
	y = false;

	k = j / i; 
	c = 'T' + (b - a);
	h = f * g;
	z = x && y;

	printf("k = %d,  c = '%c',  h = %lf,  z = %s\n", k, c, h, z == true ? "true" : "false");

	return 0;
}


	
