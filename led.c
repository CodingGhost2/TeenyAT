#include <stdio.h>
#include <stdlib.h>
#include "teenyat.h"

int main(int argc, char *argv[]) {
	FILE *bin_file = fopen("tbone.bin", "rb");
	teenyat t;
	tny_init_from_file(&t, bin_file, NULL, NULL);

	tny_word port_a;
	for ( int i=0; i <= 77; i++ ) {
		tny_clock(&t);
		tny_get_ports(&t,&port_a, NULL);
		
		if(port_a.bits.bit0 == 0) {
			printf("."); // LED Off
		}
		else {
			printf("@"); // LED On
		}
	}
	printf("\n");
	return EXIT_SUCCESS;
}