/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementation of depres v2. */
#include <cmath>
#include <algorithm>
#include "depres2.h"

#include <iostream>

using namespace std;

// #define LOG_DEBUG

/* Some macros for printing to the console */
#define PRINT_WARNING(X) (cerr << "Depres2: Warning: " << X)

#ifdef LOG_DEBUG
#define PRINT_DEBUG(X) (cout << "Depres2: Debug: " << X)
#else
#define PRINT_DEBUG(X)
#endif

namespace depres
{

Depres2IGNode::Depres2IGNode(
		SolverInterface& s,
		const std::pair<const std::string, const int> &identifier,
		const bool is_selected,
		const bool installed_automatically)
	:
		IGNode(s, identifier, is_selected, installed_automatically)
{
}

void Depres2IGNode::clear_private_data()
{
	previous_versions.clear();
}


shared_ptr<IGNode> Depres2Solver::get_or_add_node(const pair<const string, const int> &identifier)
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
				make_shared<Depres2IGNode>(*this, identifier, false, false))).first;

		active.insert(i->second.get());
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
	float cnt_ejects = 0.f;
	for (auto [id, constr] : version->get_dependencies())
	{
		auto iw = G.find(id);
		if (iw == G.end())
			continue;

		auto w = iw->second;
		if (w->chosen_version && !constr->fulfilled(
					w->chosen_version->get_source_version(),
					w->chosen_version->get_binary_version()))
		{
			cnt_ejects += 1.f;
		}
	}

	for (auto [id, constr] : version->get_pre_dependencies())
	{
		auto iw = G.find(id);
		if (iw == G.end())
			continue;

		auto w = iw->second;
		if (w->chosen_version && !constr->fulfilled(
					w->chosen_version->get_source_version(),
					w->chosen_version->get_binary_version()))
		{
			cnt_ejects += 1.f;
		}
	}

	float d = cnt_ejects > 0.f ? -1.f - (1.f - 1.f / cnt_ejects) : 0.f;

	/* Compute f */
	float f = 0.f;

	set<IGNode*> file_conflicts;
	for (const auto& file : version->get_files())
	{
		auto h = files.find_file(file);
		if (h)
			file_conflicts.insert(h->data);
	}

	if (file_conflicts.size())
		f = -1.f - (1.f - 1.f / file_conflicts.size());

	/* Compute bias b */
	/* For now implement upgrade policy only. */
	float b = (float) version_index / versions_count;

	/* Combine components into a score of the package */
	float alpha = 1.f * c + 2.f * d + 4.f * f + 1.f * b;

	PRINT_DEBUG(pv->identifier_to_string() + ":" + version->get_binary_version().to_string() <<
		" c = " << c << ", d = " << d << ", f = " << f << ", b = " << b <<
		", alpha = " << alpha << endl);

	return alpha;
}

void Depres2Solver::eject_node(IGNode& v, bool put_into_active)
{
	for (const auto& file : v.chosen_version->get_files())
		files.remove_element(file);

	v.unset_chosen_version();

	if (put_into_active)
		active.insert(&v);
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
				make_shared<Depres2IGNode>(*this, pkg->get_identifier(), false, t.second)))
			.first;

		auto node = i->second;
		node->chosen_version = node->installed_version = pkg;

		/* Add files */
		bool conflict = false;
		for (const auto& file : pkg->get_files())
		{
			if (files.find_file(file))
			{
				conflict = true;
			}
			else
			{
				files.insert_file(file);
				files.find_file(file)->data = node.get();
			}
		}

		if (conflict)
		{
			PRINT_WARNING("File conflicts in current installation found, "
				"the solution to the upgrade problem may not be accurate." << endl);
		}
	}

	/* Add dependencies */
	for (auto& p : G)
	{
		auto v = p.second;
		v->set_dependencies();

		/* Remove unsatisfying chosen versions */
		for (auto w : v->dependencies)
		{
			if (!w->version_is_satisfying())
				active.insert(w);
		}
	}

	/* Add selected packages */
	for (auto& sp : selected_packages)
	{
		auto v = get_or_add_node(sp.first);

		v->is_selected = true;
		v->installed_automatically = false;
		v->constraints.insert(make_pair(nullptr, sp.second));

		if (!v->version_is_satisfying())
			active.insert(v.get());
	}


	/* The solver algorithm's main core. */
	while (active.size())
	{
		/* Take an "arbitrary" package - for now the minimum. Note that cycle
		 * detection relies on this operation to be deterministic currently. */
		auto iv = active.begin();
		auto pv = dynamic_cast<Depres2IGNode*>(*iv);
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

		/* If the best version did not change, continue. */
		if (pv->chosen_version && pv->chosen_version == best_version)
			continue;

		/* Loop detection */
		pair<const VersionNumber, const float> loop_id(
				best_version->get_binary_version(),
				alpha_max);

		auto iloop = pv->previous_versions.find(loop_id);
		int loop_cnt = iloop != pv->previous_versions.end() ? iloop->second : 0;
		if (loop_cnt >= 10)
		{
			errors.push_back(
					"The solver considers the scenario unsolvable because it "
					"detected a loop in its execution: " +
					pv->identifier_to_string() + ":" + loop_id.first.to_string() +
					" was choosen twice with alpha = " + to_string(loop_id.second));

			return false;
		}

		pv->previous_versions.insert_or_assign(loop_id, loop_cnt + 1);

		/* Clean files of previous version */
		if (pv->chosen_version)
		{
			for (auto& file : pv->chosen_version->get_files())
				files.remove_element(file);
		}

		/* Set new version and remove/add edges */
		pv->chosen_version = best_version;
		pv->set_dependencies();
		PRINT_DEBUG("Chosen version: " << pv->chosen_version->get_binary_version().to_string() << endl);

		/* TODO: Maybe put an ejected node's previous dependencies into the
		 * active set because less constraints could allow the bias to select a
		 * 'better' version. But this will cause some oscillation when this node
		 * will be taken from the active set before other nodes that may impose
		 * different constraints...
		 *
		 * Long story short: this would require a better loop detection. */

		/* Eject newly unsatisfied dependencies and pre-dependencies */
		PRINT_DEBUG("Ejecting newly unsatisfied dependencies and pre-dependencies ..." << endl);
		for (auto w : pv->dependencies)
		{
			if (!w->version_is_satisfying())
			{
				PRINT_DEBUG("    " << w->identifier_to_string() << endl);
				eject_node(*w, false);
			}

			active.insert(w);
		}

		for (auto w : pv->pre_dependencies)
		{
			if (!w->version_is_satisfying())
			{
				PRINT_DEBUG("    " << w->identifier_to_string() << " (pre)" << endl);
				eject_node(*w, false);
			}

			active.insert(w);
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
					PRINT_DEBUG("  Ejecting dependee " << source->identifier_to_string() << "." << endl);
					eject_node(*source, true);
				}
			}
		}

		/* Eject packages which conflict by their files and insert the package's
		 * files into the current configuration (which must be valid but may be
		 * incomplete (that is may have unfulfilled dependencies but must not
		 * have conflicts)). */
		for (const auto& file : pv->chosen_version->get_files())
		{
			auto h = files.find_file(file);
			if (h && h->data->chosen_version)
			{
				PRINT_DEBUG("  Ejecting because of file conflict: " <<
					h->data->identifier_to_string() << "." << endl);

				eject_node(*(h->data), true);
			}

			files.insert_file(file);
			files.find_file(file)->data = pv;
		}
	}


	PRINT_DEBUG(installation_graph_to_dot(G) << endl);
	return true;
}

vector<string> Depres2Solver::get_errors() const
{
	return errors;
}

installation_graph_t Depres2Solver::get_G()
{
	/* Clear private data to free memory */
	for (auto [id, v] : G)
		dynamic_pointer_cast<Depres2IGNode>(v)->clear_private_data();

	files.clear();

	return move(G);
}

}
