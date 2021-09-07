#include <stdio.h>
#include "pico/stdlib.h"

int main()
{
	stdio_init_all();
	for (int i = 0; i < 10; i++) {
		printf("Hello, world!\n");
		sleep_ms(1000);
	}
	printf("Now, hit a key plz:\n");
	while (true) {
		char c = getchar();

		printf("Woo u hits %d\n", c);
	}
	return 0;
}
