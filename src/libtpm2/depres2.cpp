/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementation of depres v2. */
#include "depres2.h"

using namespace std;

namespace depres2
{

void IGNode::set_dependencies()
{
	/* Clear existing dependencies */
	for (const auto w : dependencies)
	{
		w->constraints.erase(this);
		w->reverse_dependencies.erase(this);
	}

	dependencies.clear();

	/* Add new dependencies if required */
	if (chosen_version)
	{
		auto deps = chosen_version->get_dependencies();

		for (const auto& dep : deps)
		{
			auto& w = s.get_or_add_node(dep.first);

			dependencies.push_back(&w);
			w.reverse_dependencies.insert(this);

			/* Add version constraints to neighbors */
			if (dep.second)
				w.constraints.insert(make_pair(this, dep.second));
		}
	}
}

void IGNode::unset_chosen_version()
{
	chosen_version = nullptr;
	set_dependencies();
}

void IGNode::unset_unsatisfying_version()
{
	bool fulfilled = true;

	for (auto& pc : constraints)
	{
		if (!pc.second->fulfilled(
				chosen_version->get_source_version(),
				chosen_version->get_binary_version()))
		{
			fulfilled = false;
			break;
		}
	}

	if (!fulfilled)
		unset_chosen_version();
}

IGNode &Solver::get_or_add_node(const pair<const string, const int> &identifier)
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

bool Solver::solve()
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
			w->unset_unsatisfying_version();
	}

	/* Add selected packages */
	for (auto& sp : selected_packages)
	{
		auto& v = get_or_add_node(sp.first);

		v.is_selected = true;
		v.constraints.insert(make_pair(nullptr, sp.second));
		v.unset_unsatisfying_version();
	}

	errors.push_back("Not completely implemented yet.");
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
