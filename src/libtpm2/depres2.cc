/** This file is part of the TSClient LEGACY Package Manager
 *
 * Implementation of depres v2. */
#include <cmath>
#include <algorithm>
#include <deque>
#include <stack>
#include "depres2.h"
#include "architecture.h"

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
		Depres2Solver& s,
		const std::pair<const std::string, const int> &identifier,
		const bool is_selected,
		const bool installed_automatically)
	:
		IGNode(s, identifier, is_selected, installed_automatically),
		iactive_queue(s.active_queue.end())
{
}

void Depres2IGNode::clear_private_data()
{
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
				make_shared<Depres2IGNode>(*this, identifier, false, true))).first;

		insert_into_active(i->second.get());
	}

	return i->second;
}


void Depres2Solver::insert_into_active(IGNode* _n)
{
	auto n = dynamic_cast<Depres2IGNode*>(_n);

	if (n->iactive_queue == active_queue.end())
	{
		active_queue.push_back(n);
		n->iactive_queue--;
	}
	else
	{
		/* Node in the queue; move to back */
		active_queue.splice(
				active_queue.end(),
				active_queue,
				n->iactive_queue);
	}
}

void Depres2Solver::remove_from_active(IGNode* _n)
{
	auto n = dynamic_cast<Depres2IGNode*>(_n);
	if (n->iactive_queue != active_queue.end())
	{
		active_queue.erase(n->iactive_queue);
		n->iactive_queue = active_queue.end();
	}
}


/* @param version_index  \in [0, versions_count), bigger for newer version
 * numbers */
float Depres2Solver::compute_alpha(
		const pair<const string, const int> &id,
		const IGNode* pv,
		shared_ptr<PackageVersion> version,
		int version_index, int versions_count)
{
	/* Compute c */
	float c = 0.f;
	float conflict = false;
	bool user_pinning = false;
	bool user_selected = false;

	{
		unsigned t = 0;

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
			{
				conflict = true;

				if (source)
				{
					auto t_w = dynamic_cast<const Depres2IGNode*>(source)->t_eject;
					if (t_w > t)
						t = t_w;
				}
			}
		}

		/* If an arbitrary version is user selected, this version could be
		 * chosen. */
		if (pv->is_selected && !user_pinning)
			user_selected = true;

		if (conflict)
		{
			/* Do not choose versions that contradict user-specified
			 * constraints. */
			if (user_pinning && !user_selected)
			{
				c = -INFINITY;
			}
			else if (user_selected)
			{
				c = -1;
			}
			else
			{
				/* Prefer the version which did least recently eject a parent */
				float theta = t_now >= t ? (1.f / (t_now - t + 1)) : 0.f;
				c = -9 - theta;
			}
		}
		else
		{
			c = user_selected ? 1 : 0;
		}
	}

	/* Compute d */
	/* Test if the version can be installed without ejecting other
	 * package versions (of dependencies or pre-dependencies). */
	float cnt_ejects = 0.f;
	unsigned t = 0;
	for (auto& [id, constr] : version->get_dependencies())
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

			auto t_w = dynamic_pointer_cast<Depres2IGNode>(w)->t_eject;
			if (t_w > t)
				t = t_w;
		}
	}

	for (auto& [id, constr] : version->get_pre_dependencies())
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

			auto t_w = dynamic_pointer_cast<Depres2IGNode>(w)->t_eject;
			if (t_w > t)
				t = t_w;
		}
	}

	float mu = 1.f - 1.f / (cnt_ejects + 1);
	/* t_now - t < 0 => overflow => eject happened a long time ago */
	float theta = t_now >= t ? (1.f / (t_now - t + 1)) : 0.f;
	PRINT_DEBUG("theta = " << theta << endl);
	// float d = mu > 0.f ? (-1.f - .3125f * mu - .5f * theta) : 0.f;
	float d = mu > 0.f ? (-1.f - .0625f * mu - .5f * theta) : 0.f;

	/* Compute f */
	float f = 0.f;

	set<IGNode*> file_conflicts;
	for (const auto& file : version->get_files())
	{
		auto h = files.find_file(file);

		/* Ignore conflicts with a different version of this package */
		if (h && h->data != pv)
		{
			PRINT_DEBUG("  file conflict: " << file << " (by " << h->data->get_name()
					<< ")" << endl);

			file_conflicts.insert(h->data);
		}
	}

	if (file_conflicts.size())
		f = -1.f - (1.f - 1.f / file_conflicts.size());

	/* Compute bias b */
	float b;

	switch (policy)
	{
	case Policy::upgrade:
		b = (float) version_index / versions_count;
		break;

	case Policy::strong_selective_upgrade:
		if (pv->is_selected)
		{
			b = ((float) version_index + 0.9f) / versions_count;
			b = (b * b * b) * 50.f;
		}
		else
		{
			b = (float) version_index / versions_count;
		}
		break;

	case Policy::keep_newer:
	default:
		if (pv->installed_version && *(pv->installed_version) == *(version))
			b = 0.95f;
		else
			b = 0.8f * ((float) version_index / versions_count);
		break;
	}

	/* Combine components into a score of the package */
	float alpha = 1000.f * c + 2.f * d + 8.f * f + .2f * b;

	PRINT_DEBUG(pv->identifier_to_string() + ":" + version->get_binary_version().to_string() <<
		" c = " << c << ", d = " << d << ", f = " << f << ", b = " << b <<
		", alpha = " << alpha << endl);

	return alpha;
}

void Depres2Solver::eject_node(IGNode& v, bool put_into_active)
{
	static_cast<Depres2IGNode*>(&v)->t_eject = t_now;

	if (v.chosen_version)
	{
		for (const auto& file : v.chosen_version->get_files())
			files.remove_element(file);
	}

	v.unset_chosen_version();

	if (put_into_active)
		insert_into_active(&v);
}

bool Depres2Solver::is_node_unreachable(IGNode& v)
{
	/* Keep installed- and selected nodes */
	if (v.is_selected || v.installed_version)
		return false;

	return v.reverse_dependencies.size() == 0 &&
		v.reverse_pre_dependencies.size() == 0;
}

void Depres2Solver::remove_unreachable_nodes()
{
	deque<shared_ptr<IGNode>> to_remove;
	for (auto& [id, v] : G)
	{
		if (is_node_unreachable(*v))
			to_remove.push_back(v);
	}

	while (to_remove.size())
	{
		auto v = to_remove.front();
		to_remove.pop_front();

		PRINT_DEBUG("Garbage collecting node " << v->identifier_to_string() << "." << endl);

		auto dependencies = v->dependencies;
		dependencies.insert(v->dependencies.end(),
				v->pre_dependencies.begin(), v->pre_dependencies.end());

		/* Remove node */
		eject_node(*v, false);

		remove_from_active(v.get());
		G.erase(v->identifier);

		/* Check if the node's dependencies became unreachable */
		for (auto u : dependencies)
		{
			if (is_node_unreachable(*u))
				to_remove.push_back(G.find(u->identifier)->second);
		}
	}
}

void Depres2Solver::format_loop_error_message()
{
	/* loop_cnt, name, arch, version, alpha */
	vector<tuple<int, string, int, VersionNumber, float>> pkgs;

	for (auto& [k,v] : previous_versions)
	{
		if (v > 8)
			pkgs.push_back({v, get<0>(k), get<1>(k), get<2>(k), get<3>(k)});
	}

	sort(pkgs.begin(), pkgs.end());

	errors.push_back("Package versions probably causing the algorithm's oscillation:");
	for (auto& [loop_cnt, name, arch, version, alpha] : pkgs)
	{
		errors.push_back(
				"  " + name + "@" + Architecture::to_string(arch) + ":" + version.to_string() +
				" loop_cnt: " + to_string(loop_cnt) + ", alpha: " + to_string(alpha));
	}
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

void Depres2Solver::set_policy(int p)
{
	if (
			p != Policy::keep_newer &&
			p != Policy::upgrade &&
			p != Policy::strong_selective_upgrade
	   )
	{
		return;
	}

	policy = p;
}

void Depres2Solver::set_evaluate_all(bool enabled)
{
	evaluate_all = enabled;
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
				insert_into_active(w);
		}
	}

	/* Add selected packages */
	for (auto& sp : selected_packages)
	{
		auto v = get_or_add_node(sp.first);

		v->is_selected = true;
		v->installed_automatically = false;

		if (sp.second)
			v->constraints.insert(make_pair(nullptr, sp.second));

		if (!v->version_is_satisfying() || policy == Policy::strong_selective_upgrade)
			insert_into_active(v.get());
	}

	/* If requested, eject all all installed packages. */
	if (evaluate_all)
	{
		for (auto &t : installed_packages)
			eject_node(*get_or_add_node(t.first->get_identifier()), true);
	}


	/* The solver algorithm's main core. */
	while (!active_queue.empty())
	{
		t_now++;

		/* Take the package at the active queue's front. Note that cycle
		 * detection relies on this operation to be deterministic currently.
		 * However I'm not sure if all other assumptions of cycle detection
		 * still hold... */
		auto pv = active_queue.front();
		remove_from_active(pv);

		/* Don't evaluate a package that was marked for removal. */
		if (pv->marked_for_removal)
			continue;

		PRINT_DEBUG("Considering node `" << pv->identifier_to_string() << "' ..." << endl);

		/* Find the best-fitting version for the active package */
		auto version_numbers = cb_list_package_versions(pv->get_name(), pv->get_architecture());
		sort(version_numbers.begin(), version_numbers.end(),
				[](auto& a, auto& b) { return a > b; });

		int i = version_numbers.size();
		float alpha_max = -INFINITY;
		float alpha_installed = 0.f;
		shared_ptr<PackageVersion> best_version = nullptr;

		for (auto& version_number : version_numbers)
		{
			i--;

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

			if (pv->installed_version && *(pv->installed_version) == *version)
				alpha_installed = alpha;

			/* Choose version with highest alpha */
			if (alpha > alpha_max)
			{
				alpha_max = alpha;
				best_version = version;

				PRINT_DEBUG("    -> new max" << endl);
			}
		}

		if (i == (int) version_numbers.size())
		{
			errors.push_back("Could not find version for " +
					pv->identifier_to_string() + ".");
			return false;
		}

		/* Require user selected versions */
		if (!best_version || alpha_max < -100000.f)
		{
			errors.push_back("Could not find suitable version for " +
					pv->identifier_to_string() + ".");
			return false;
		}

		/* If the best version did not change, continue. */
		if (pv->chosen_version && *(pv->chosen_version) == *(best_version))
			continue;

		/* Loop detection */
		tuple<const string, const int, const VersionNumber, const float> loop_id(
				pv->get_name(),
				pv->get_architecture(),
				best_version->get_binary_version(),
				alpha_max);

		auto iloop = previous_versions.find(loop_id);
		int loop_cnt = iloop != previous_versions.end() ? iloop->second : 0;
		if (loop_cnt >= 10)
		{
			errors.push_back(
					"The solver considers the scenario unsolvable because it "
					"detected a loop in its execution: " +
					pv->identifier_to_string() + ":" + get<2>(loop_id).to_string() +
					" was choosen twice with alpha = " + to_string(get<3>(loop_id)));

			format_loop_error_message();

			return false;
		}

		previous_versions.insert_or_assign(loop_id, loop_cnt + 1);

		/* Clean files of previous version */
		if (pv->chosen_version)
		{
			for (auto& file : pv->chosen_version->get_files())
				files.remove_element(file);
		}

		/* If the package is installed and conflicts with another package, is
		 * not selected, and no 'better' version has been found than the
		 * installed one, mark the package for removal. */
		if (pv->installed_version && !pv->is_selected)
		{
			/* !is_selected -> c = 0 */
			/* New chosen version has a file conflict or the installed version
			 * has a file conflict and the newly chosen version is not newer.
			 *
			 * Note that hardcoding the requirement for a newer version here
			 * breaks with the score-based approach to some degree and enforces
			 * a specific behavior on the build system: when files are moved
			 * from a package, the new version of the package (having fewer
			 * files) will have a newer version number. However this should be
			 * fine as long as relative time is strictly monotonic. */
			if (
					alpha_max < -6.5f || (
						alpha_installed < -6.5f &&
						alpha_installed > - 31.f &&
						*best_version < *(pv->installed_version)
					)
			   )
			{
				/* Only remove this node if there is no selected depender. */
				/* Do a DFS traversal */
				bool selected_dependee = false;

				/* having a visited set is a bad but pragmatic workaround. */
				set<IGNode*> visited;
				stack<IGNode*> s;
				s.push(pv);
				visited.insert(pv);

				while (!s.empty() && !selected_dependee)
				{
					auto v = s.top();
					s.pop();

					for (auto u : v->reverse_dependencies)
					{
						/* If we are evaluating all packages, treet the manually
						 * installed packages as selected, too. */
						if (u->is_selected || (evaluate_all && !u->installed_automatically))
						{
							selected_dependee = true;
							break;
						}
						else if (visited.find(u) == visited.end())
						{
							s.push(u);
							visited.insert(u);
						}
					}

					if (selected_dependee)
						break;

					for (auto u : v->reverse_pre_dependencies)
					{
						if (u->is_selected || (evaluate_all && !u->installed_automatically))
						{
							selected_dependee = true;
							break;
						}
						else if (visited.find(u) == visited.end())
						{
							s.push(u);
							visited.insert(u);
						}
					}
				}

				if (!selected_dependee)
				{
					PRINT_DEBUG("  marking for removal: alpha_max = " << alpha_max <<
							" / alpha_installed: " << alpha_installed << endl);

					eject_node(*pv, false);
					pv->marked_for_removal = true;
					continue;
				}
			}
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
			if (dynamic_cast<Depres2IGNode*>(w)->marked_for_removal)
			{
				pv->marked_for_removal = true;
			}
			else
			{
				/* Don't eject the currently considered node in case of a cycle
				 * */
				if (w == pv)
					continue;

				if (w->chosen_version && !w->version_is_satisfying())
				{
					PRINT_DEBUG("    " << w->identifier_to_string() << endl);
					eject_node(*w, false);
				}

				insert_into_active(w);
			}
		}

		for (auto w : pv->pre_dependencies)
		{
			if (dynamic_cast<Depres2IGNode*>(w)->marked_for_removal)
			{
				pv->marked_for_removal = true;
			}
			else
			{
				/* Don't eject the currently considered node in case of a cycle
				 * */
				if (w == pv)
					continue;

				if (w->chosen_version && !w->version_is_satisfying())
				{
					PRINT_DEBUG("    " << w->identifier_to_string() << " (pre)" << endl);
					eject_node(*w, false);
				}

				insert_into_active(w);
			}
		}

		if (pv->marked_for_removal)
		{
			/* A dependency is marked for removal, hence this package was marked
			 * for removal, too. Eject the package */
			eject_node(*pv, false);
		}
		else
		{
			/* If necessarry, eject conflicting dependees as well */
			/* A dependee can only be conflicting if it imposes version constraints
			 * or has common files. In both cases it can be detected without reverse
			 * edges. */
			if (alpha_max < 0.f)
			{
				/* Try to not always eject the same node. */
				vector<pair<IGNode*, std::shared_ptr<const PackageConstraints::Formula>>>
					constrs(pv->constraints.begin(), pv->constraints.end());

				int new_eject_index = -1;

				for (unsigned i = pv->eject_index; i < pv->eject_index + constrs.size(); i++)
				{
					auto [source, constr] = constrs[i % constrs.size()];
					if (source && !constr->fulfilled(
								pv->chosen_version->get_source_version(),
								pv->chosen_version->get_binary_version()))
					{
						PRINT_DEBUG("  Ejecting dependee " << source->identifier_to_string() << "." << endl);
						eject_node(*source, true);

						if (new_eject_index < 0)
							new_eject_index = i;
					}
				}

				if (new_eject_index >= 0)
					pv->eject_index += new_eject_index;
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

		/* Trigger garbage collection */
		remove_unreachable_nodes();
	}

	/* Trigger garbage collection in case the last loop run did not. */
	remove_unreachable_nodes();


	/* Remove nodes that were marked for removal */
	bool erase_error = false;
	for (auto i = G.begin(); i != G.end();)
	{
		auto v = dynamic_pointer_cast<Depres2IGNode>(i->second);
		i++;

		if (v->marked_for_removal)
		{
			/* Check that the node has no chosen version and no incoming edges
			 * (=is unreachable) */
			if (v->chosen_version)
			{
				errors.push_back("Node " + v->identifier_to_string() +
						" is marked for removal but has a chosen version.");
				return false;
			}

			if (v->reverse_dependencies.size() || v->reverse_pre_dependencies.size())
			{
				errors.push_back("Node " + v->identifier_to_string() +
						" is marked for removal but has incoming edges.");
				return false;
			}

			/* If the node was selected, print an error message and fail the
			 * computation. */
			if (v->is_selected)
			{
				errors.push_back("Package " + v->identifier_to_string() +
						" is selected by the user but cannot be installed "
						"because it conflicts with another package (or its "
						"dependencies conflict).");

				erase_error = true;
			}

			G.erase(v->identifier);
		}
	}

	if (erase_error)
		return false;

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
	for (auto& [id, v] : G)
		dynamic_pointer_cast<Depres2IGNode>(v)->clear_private_data();

	files.clear();
	previous_versions.clear();

	return move(G);
}

}
