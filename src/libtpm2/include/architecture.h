/* This module handles different architectures */

#ifndef __ARCHITECTURE_H
#define __ARCHITECTURE_H

#include <exception>
#include <string>

class Architecture
{
public:
	/* Good for parsing files etc. */
	static const int invalid = -1;
	static const int amd64 = 0;
	static const int i386 = 1;

	static const std::string to_string(const int);
	static int from_string(const std::string&);
	static int from_string(const char*);
};

class InvalidArchitecture : public std::exception
{
protected:
	std::string msg;

public:
	InvalidArchitecture(const int);
	InvalidArchitecture(const std::string&);

	const char* what() const noexcept override;
};

#endif /* __ARCHITECTURE_H */
