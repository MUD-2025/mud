#include <arpa/inet.h>
#include <netdb.h>

#include "engine/core/comm.h"

long prool_boot_time;

int main(const int argc, char **argv) {

	prool_boot_time=time(0);

	return main_function(argc, argv);
}

#include "prool.c" // prools code

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
