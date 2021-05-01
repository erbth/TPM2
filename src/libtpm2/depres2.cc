/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementation of depres v2. */
#include "depres2.h"

using namespace std;

namespace depres
{

IGNode &Depres2Solver::get_or_add_node(const pair<const string, const int> &identifier)
{
	auto i = G.find(identifier);

	if (i == G.end())
	{
		i = G.insert(make_pair(
				identifier,
				IGNode(*this, identifier, false))).first;

		active.insert(&(i->second));
	}

	return i->second;
}

void Depres2Solver::set_parameters(
		const vector<shared_ptr<PackageVersion>> &installed_packages,
		const vector<selected_package_t> &selected_packages,
		cb_list_package_versions_t cb_list_package_versions,
		cb_get_package_version_t cb_get_package_version)
{
	this->installed_packages = installed_packages;
	this->selected_packages = selected_packages;
	this->cb_list_package_versions = cb_list_package_versions;
	this->cb_get_package_version = cb_get_package_version;
}

bool Depres2Solver::solve()
{
	/* Insert the installed packages into the installation graph */
	for (auto &pkg : installed_packages)
	{
		auto i = G.insert(make_pair(
				pkg->get_identifier(),
				IGNode(*this, pkg->get_identifier(), false)))
			.first;

		auto &node = i->second;
		node.chosen_version = node.installed_version = pkg;
	}

	/* Add dependencies */
	for (auto p : G)
	{
		auto& v = p.second;
		v.set_dependencies();

		/* Remove unsatisfying chosen versions */
		for (auto w : v.dependencies)
		{
			if (w->unset_unsatisfying_version())
				active.insert(w);
		}
	}

	/* Add selected packages */
	for (auto& sp : selected_packages)
	{
		auto& v = get_or_add_node(sp.first);

		v.is_selected = true;
		v.constraints.insert(make_pair(nullptr, sp.second));

		if (v.unset_unsatisfying_version())
			active.insert(&v);
	}

	/* As long as there are active packages, try another configuration. */
	while (active.size())
	{
		/* Choose a node */
		auto v = *(active.first());

		/* Choose a version that satisfies the dependencies, 
	}

	errors.push_back("Not completely implemented yet.");
	return true;
}

vector<string> Depres2Solver::get_errors() const
{
	return errors;
}

installation_graph_t Depres2Solver::get_G()
{
	return move(G);
}

}
