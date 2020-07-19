/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementation of depres v2. */
#include "depres2.h"

using namespace std;

namespace depres2
{

IGNode &Solver::get_or_add_node(const pair<const string, const int> &identifier)
{
	auto i = G.find(identifier);

	if (i == G.end())
	{
		i = G.insert(make_pair(
				identifier,
				false)).second;
	}

	return &i->second;
}

bool Solver::solve()
{
	/* Insert the installed packages into the installation graph */
	for (auto &pkg : installed_packages)
	{
		auto i = G.insert(make_pair(
				pkg->get_identifier(),
				IGNode(pkg->get_identifier(), false)))
			.second;

		auto &node = i->second;
		node->chosen_version = node->installed_version = pkg;
	}

	/* Add dependencies */
	for (auto &pkg : installed_packages)
	{
		alksfj
	}

	return false;
}

vector<string> Solver::retrieve_errors() const
{
	return errors;
}

Solver::installation_graph_t Solver::get_G()
{
	return move(G);
}

}
