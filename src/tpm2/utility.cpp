#include "utility.h"

using namespace std;


void print_target(shared_ptr<Parameters> params, bool to_stderr)
{
	FILE* stream = to_stderr ? stderr : stdout;

	if (params->target_is_native())
		fprintf (stream, "Runtime system is native\n");
	else
		fprintf (stream, "Runtime system is at \"%s\"\n", params->target.c_str());
}
