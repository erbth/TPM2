/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementations of methods and functions common to all solvers of the depres
 * class. */
#include "depres_common.h"

using namespace std;

namespace depres
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

bool IGNode::unset_unsatisfying_version()
{
	if (!chosen_version)
		return false;

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
	{
		unset_chosen_version();
		return true;
	}
	else
	{
		return false;
	}
}

}
