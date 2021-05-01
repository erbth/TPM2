#include <cstring>
#include <iostream>
#include "safe_console_input.h"

extern "C" {
#include <ctype.h>
}

using namespace std;


char safe_query_user_input (const string& options)
{
	char buf[1024] = "";
	char def = 0;
	string text = "[";

	bool first = false;

	for (auto c : options)
	{
		if (first)
			text += '/';
		else
			first = true;

		text += c;

		if (isupper(c))
			def = tolower(c);
	}

	text += "]";

	for (;;)
	{
		cout << text << flush;

		if (cin.fail())
			return -1;

		cin.getline (buf, sizeof (buf) / sizeof (*buf));

		if (cin.fail())
			return -1;

		auto buf_len = strlen (buf);

		if (buf_len == 1)
		{
			char c = buf[0];

			for (auto c2 : options)
			{
				if (tolower(c2) == c)
					return c;
			}
		}
		else if (buf_len == 0 && def)
		{
			return def;
		}
	}
}
