#include "parameters.h"

using namespace std;


bool Parameters::target_is_native() const
{
	return target == "/";
}
