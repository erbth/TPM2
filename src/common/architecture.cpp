#include "architecture.h"

using namespace std;


const string Architecture::to_string(int a)
{
	switch (a)
	{
		case amd64:
			return "amd64";

		case i386:
			return "i386";

		default:
			throw InvalidArchitecture(a);
	}
}

int Architecture::from_string(const string &a)
{
	if (a == "amd64")
		return amd64;

	else if (a == "i386")
		return i386;

	throw InvalidArchitecture(a);
}


InvalidArchitecture::InvalidArchitecture(const int a)
{
	msg = "Invalid architecture with integer value " + to_string(a) + ".";
}

InvalidArchitecture::InvalidArchitecture(const string &a)
{
	msg = "Invalid architecture specifying string \"" + a + "\".";
}

const char* InvalidArchitecture::what() const noexcept
{
	return msg.c_str();
}
