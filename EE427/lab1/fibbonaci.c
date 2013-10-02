#include <stdio.h>
#include "platform.h"

void print(char *str);

int main() {

	init_platform();
	int a;
	int b = 0;
	int c = 1;
	xil_printf("\n%d ",b);
	xil_printf("%d ",c);

	int next;
	for (a = 0; a < 20; a++) {
		next = b + c;
		xil_printf("%d ",next);
		b = c;
		c = next;
	}

	cleanup_platform();

	return 0;

}
