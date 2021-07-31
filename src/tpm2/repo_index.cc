#include "repo_index.h"

using namespace std;

RepoIndex::~RepoIndex()
{
}


IndexAuthenticationFailed::IndexAuthenticationFailed(const std::string msg)
	: msg(msg)
{
}

const char* IndexAuthenticationFailed::what() const noexcept
{
	return msg.c_str();
}

IndexAuthenticationFailedNoSignature::IndexAuthenticationFailedNoSignature(const std::string msg)
	: IndexAuthenticationFailed(msg)
{
}

UnsupportedIndexVersion::UnsupportedIndexVersion(const std::string msg)
	: msg(msg)
{
}

const char* UnsupportedIndexVersion::what() const noexcept
{
	return msg.c_str();
}
