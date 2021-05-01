#include "dependencies.h"

using namespace std;


Dependency::Dependency(const string &name, const int architecture,
		const shared_ptr<const PackageConstraints::Formula> version_formula)
	:
		identifier({name, architecture}), version_formula(version_formula)
{
}

const string& Dependency::get_name() const
{
	return identifier.first;
}

int Dependency::get_architecture() const
{
	return identifier.second;
}

bool Dependency::operator<(const Dependency& o) const
{
	return identifier < o.identifier;
}


set<Dependency>::iterator DependencyList::begin()
{
	return dependencies.begin();
}

set<Dependency>::iterator DependencyList::end()
{
	return dependencies.end();
}


set<Dependency>::const_iterator DependencyList::cbegin() const
{
	return dependencies.cbegin();
}

set<Dependency>::const_iterator DependencyList::cend() const
{
	return dependencies.cend();
}
