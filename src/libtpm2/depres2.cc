/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementation of depres v2. */
#include <cmath>
#include <algorithm>
#include "depres2.h"

#include <iostream>

using namespace std;

namespace depres
{

IGNode &Depres2Solver::get_or_add_node(const pair<const string, const int> &identifier)
{
	auto i = G.find(identifier);

	if (i == G.end())
	{
		/* Currently installed nodes will not be removed from the graph until
		 * the core part of the algorithm has finished and will be inserted
		 * first. Moreover packages requested by the user will be installed
		 * first. Hence packages inserted because other packages depend on them
		 * (i.e. when this method is called) will always be installed
		 * automatically.
		 *
		 * Actually this method will be called when inserting manually specified
		 * packages, too, but the two attributes will be set accordingly. */
		i = G.insert(make_pair(
				identifier,
				IGNode(*this, identifier, false, false))).first;

		active.insert(&(i->second));
	}

	return i->second;
}


float Depres2Solver::compute_alpha(
		const pair<const string, const int> &id,
		const IGNode* pv,
		shared_ptr<PackageVersion> version,
		int version_index, int versions_count)
{
	/* Compute c */
	float conflict = false;
	bool user_pinning = false;
	bool user_selected = false;

	for (auto [source, constr] : pv->constraints)
	{
		if (source == nullptr)
			user_pinning = true;

		if (constr->fulfilled(
					version->get_source_version(),
					version->get_binary_version()))
		{
			/* Prefer user selected version */
			if (source == nullptr)
				user_selected = true;
		}
		else
			conflict = true;
	}

	/* If an arbitrary version is user selected, this version could be
	 * chosen. */
	if (pv->is_selected && !user_pinning)
		user_selected = true;

	float c = 0.f;
	if (conflict)
		c = user_selected ? -1000 : -INFINITY;
	else
		c = user_selected ? 1000 : 0;

	/* Compute d */
	/* Test if the version can be installed without ejecting other
	 * package versions (of dependencies or pre-dependencies). */
	bool ejects = false;
	for (auto [id, constr] : version->get_dependencies())
	{
		auto iw = G.find(id);
		if (iw == G.end())
			continue;

		auto& w = iw->second;
		if (w.chosen_version && !constr->fulfilled(
					w.chosen_version->get_source_version(),
					w.chosen_version->get_binary_version()))
		{
			ejects = true;
			break;
		}
	}

	for (auto [id, constr] : version->get_pre_dependencies())
	{
		auto iw = G.find(id);
		if (iw == G.end())
			continue;

		auto& w = iw->second;
		if (w.chosen_version && !constr->fulfilled(
					w.chosen_version->get_source_version(),
					w.chosen_version->get_binary_version()))
		{
			ejects = true;
			break;
		}
	}

	float d = ejects ? -1.f : 0.f;

	/* Compute bias b */
	/* For now implement upgrade policy only. */
	float b = (float) version_index / versions_count;

	/* Combine components into a score of the package */
	float alpha = 1.f * c + 2.f * d + 1.f * b;

	cout << pv->identifier_to_string() + ":" + version->get_binary_version().to_string() <<
		" c = " << c << ", d = " << d << ", b = " << b << ", alpha = " << alpha << endl;

	return alpha;
}


void Depres2Solver::set_parameters(
		const vector<pair<shared_ptr<PackageVersion>,bool>> &installed_packages,
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
	for (auto &t : installed_packages)
	{
		auto &pkg = t.first;
		auto i = G.insert(make_pair(
				pkg->get_identifier(),
				IGNode(*this, pkg->get_identifier(), false, t.second)))
			.first;

		auto &node = i->second;
		node.chosen_version = node.installed_version = pkg;
	}

	/* Add dependencies */
	for (auto& p : G)
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
		v.installed_automatically = false;
		v.constraints.insert(make_pair(nullptr, sp.second));

		if (v.unset_unsatisfying_version())
			active.insert(&v);
	}

	// cout << "Active set:" << endl;
	// for (auto w : active)
	// 	cout << "    " << w->identifier.first << endl;


	/* The solver algorithm's main core. */
	while (active.size())
	{
		/* Take an arbitrary package */
		auto iv = active.begin();
		auto pv = *iv;
		active.erase(iv);

		/* Find the best-fitting version for the active package */
		auto version_numbers = cb_list_package_versions(pv->get_name(), pv->get_architecture());
		sort(version_numbers.begin(), version_numbers.end());

		int i = -1;
		float alpha_max = -INFINITY;
		shared_ptr<PackageVersion> best_version = nullptr;

		for (auto& version_number : version_numbers)
		{
			i++;

			auto version = cb_get_package_version(pv->get_name(), pv->get_architecture(), version_number);
			if (!version)
			{
				errors.push_back("Version " + version_number.to_string() +
						" of package " + pv->identifier_to_string() +
						" disappeared while solving.");
				return false;
			}

			auto alpha = compute_alpha(
					pv->identifier,
					pv,
					version, i, version_numbers.size());

			/* Choose version with highest alpha */
			if (alpha > alpha_max)
			{
				alpha_max = alpha;
				best_version = version;
			}
		}

		/* Require user selected versions */
		if (!best_version)
		{
			errors.push_back("Could not find version for " +
					pv->identifier_to_string() + ".");
			return false;
		}

		if (alpha_max < -10000.f)
		{
			errors.push_back("Could not find suitable version for " +
					pv->identifier_to_string() + ".");
			return false;
		}

		pv->chosen_version = best_version;
		pv->set_dependencies();
		cout << "Chosen version: " << pv->chosen_version->get_binary_version().to_string() << endl;

		/* Eject newly unsatisfied dependencies and pre-dependencies */
		cout << "Ejecting newly unsatisfied dependencies and pre-dependencies ..." << endl;
		for (auto w : pv->dependencies)
		{
			if (w->unset_unsatisfying_version())
			{
				cout << "    " << w->identifier_to_string() << endl;
				active.insert(w);
			}
		}

		for (auto w : pv->pre_dependencies)
		{
			if (w->unset_unsatisfying_version())
			{
				cout << "    " << w->identifier_to_string() << " (pre)" << endl;
				active.insert(w);
			}
		}

		/* If necessarry, eject conflicting dependees as well */
		/* A dependee can only be conflicting if it imposes version constraints
		 * or has common files. In both cases it can be detected without reverse
		 * edges. */
		if (alpha_max < 0.f)
		{
			auto constrs = pv->constraints;
			for (auto [source, constr] : constrs)
			{
				if (source && !constr->fulfilled(
							pv->chosen_version->get_source_version(),
							pv->chosen_version->get_binary_version()))
				{
					cout << "  Ejecting dependee " << source->identifier_to_string() << "." << endl;
					source->unset_chosen_version();
					active.insert(source);
				}
			}
		}
	}


	cout << installation_graph_to_dot(G) << endl;
	// errors.push_back("Not completely implemented yet.");
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
