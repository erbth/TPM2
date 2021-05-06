/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementations of methods and functions common to all solvers of the depres
 * class. */
#include "architecture.h"
#include "depres_common.h"

#include <iostream>

using namespace std;

namespace depres
{

IGNode::IGNode(
		SolverInterface& s,
		const std::pair<const std::string, const int> &identifier,
		const bool is_selected,
		const bool installed_automatically)
	:
		s(s),
		identifier(identifier),
		is_selected(is_selected),
		installed_automatically(is_selected ? false : installed_automatically)
{
}

IGNode::~IGNode()
{
}

string IGNode::identifier_to_string() const
{
	return identifier.first + "@" + Architecture::to_string(identifier.second);
}

string IGNode::get_name() const
{
	return identifier.first;
}

int IGNode::get_architecture() const
{
	return identifier.second;
}

void IGNode::set_dependencies()
{
	/* Clear existing dependencies and pre-dependencies */
	for (const auto w : dependencies)
	{
		w->constraints.erase(this);
		w->reverse_dependencies.erase(this);
	}

	for (const auto w : pre_dependencies)
	{
		w->constraints.erase(this);
		w->reverse_pre_dependencies.erase(this);
	}

	dependencies.clear();
	pre_dependencies.clear();

	/* Add new dependencies if required */
	if (chosen_version)
	{
		auto deps = chosen_version->get_dependencies();

		for (const auto& dep : deps)
		{
			auto pw = s.get_or_add_node(dep.first);

			dependencies.push_back(pw.get());
			pw->reverse_dependencies.insert(this);

			/* Add version constraints to neighbors */
			if (dep.second)
				pw->constraints.insert(make_pair(this, dep.second));
		}

		auto pre_deps = chosen_version->get_pre_dependencies();

		for (const auto& dep : pre_deps)
		{
			auto pw = s.get_or_add_node(dep.first);

			pre_dependencies.push_back(pw.get());
			pw->reverse_pre_dependencies.insert(this);

			/* Add version constraints to neighbors or add it to an already
			 * present version constraint from dependency. */
			if (dep.second)
			{
				auto i = pw->constraints.find(this);
				if (i != pw->constraints.end())
				{
					i->second = make_shared<PackageConstraints::And>(i->second, dep.second);
				}
				else
				{
					pw->constraints.insert(make_pair(this, dep.second));
				}
			}
		}
	}
}

void IGNode::unset_chosen_version()
{
	chosen_version = nullptr;
	set_dependencies();
}

bool IGNode::version_is_satisfying()
{
	if (!chosen_version)
		return true;

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

	return fulfilled;
}

bool IGNode::unset_unsatisfying_version()
{
	if (!chosen_version)
		return false;

	if (!version_is_satisfying())
	{
		unset_chosen_version();
		return true;
	}
	else
	{
		return false;
	}
}

string installation_graph_to_dot(installation_graph_t &G, const string &name)
{
	string dot;

	dot += "digraph " + name + " {\n";

	/* Assign each node an index */
	int i = 0;
	for (auto& p : G)
		p.second->algo_priv = i++;

	/* Render node */
	for (auto p : G)
	{
		auto v = p.second;
		auto cv = v->chosen_version;
		auto iv = v->installed_version;

		dot += "    " + to_string(v->algo_priv) + " [label=\"";

		if (!iv)
		{
			dot += "missing(" + v->get_name() +
				"@" + Architecture::to_string(v->get_architecture()) +
				":" + (cv ? cv->get_binary_version().to_string() : "<none>") +
				", " + (v->installed_automatically ? "auto" : "manual") +
				")";
		}
		else if (iv == cv)
		{
			dot += "installed(" + v->get_name() +
				"@" + Architecture::to_string(v->get_architecture()) +
				":" + (cv ? cv->get_binary_version().to_string() : "<none>") +
				", " + (v->installed_automatically ? "auto" : "manual") +
				")";
		}
		else
		{
			dot += "wrong version(" + v->get_name() +
				"@" + Architecture::to_string(v->get_architecture()) +
				":" + (cv ? cv->get_binary_version().to_string() : "<none>") +
				", " + (v->installed_automatically ? "auto" : "manual") +
				")";
		}

		dot += "\"];\n";
	}

	/* Render dependencies and pre-dependencies */
	for (auto [id, v] : G)
	{
		for (auto d : v->pre_dependencies)
			dot += "    " + to_string(v->algo_priv) + " -> " + to_string(d->algo_priv) + " [style=dotted];\n";

		for (auto d : v->dependencies)
			dot += "    " + to_string(v->algo_priv) + " -> " + to_string(d->algo_priv) + ";\n";
	}

	dot += "}\n";

	return dot;
}

}
