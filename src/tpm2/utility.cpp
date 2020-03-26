#include <cstdio>
#include "utility.h"

using namespace std;


void print_target(shared_ptr<Parameters> params)
{
	if (params->target_is_native())
		printf ("Runtime system is native\n");
	else
		printf ("Runtime system is at \"%s\"\n", params->target.c_str());
}
