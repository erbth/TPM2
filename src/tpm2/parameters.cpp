#include <cstdlib>
#include <filesystem>
#include "parameters.h"
#include "utility.h"

using namespace std;
namespace fs = std::filesystem;


bool Parameters::target_is_native() const
{
	return target == "/";
}


void Parameters::read_from_env()
{
	const char* v = secure_getenv ("TPM_TARGET");

	if (v)
		target = get_absolute_path(v);
}
