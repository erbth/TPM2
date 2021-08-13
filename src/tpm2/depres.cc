#include <algorithm>
#include <set>
#include "depres.h"
#include "architecture.h"
#include "common_utilities.h"
#include "depres_factory.h"
#include "depres2.h"
#include "file_trie.h"

using namespace std;


namespace depres
{
/* InstalledPackageVersion to provide installed packages to the solver */
InstalledPackageVersion::InstalledPackageVersion(
		shared_ptr<PackageMetaData> mdata,
		PackageDB& pkgdb)
	:
		PackageVersion(mdata->name, mdata->architecture, mdata->source_version, mdata->version),
		mdata(mdata),
		pkgdb(pkgdb)
{
	for (const auto& file : pkgdb.get_files(mdata))
	{
		if (file.type == FILE_TYPE_DIRECTORY)
			directory_paths.push_back(file.path);
		else
			file_paths.push_back(file.path);
	}
}

bool InstalledPackageVersion::is_installed() const
{
	return true;
}

vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>>
InstalledPackageVersion::get_dependencies()
{
	vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>> deps;
	for (auto& dep : mdata->dependencies)
		deps.push_back(make_pair(dep.identifier, dep.version_formula));

	return deps;
}

vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>>
InstalledPackageVersion::get_pre_dependencies()
{
	vector<pair<pair<string, int>, shared_ptr<const PackageConstraints::Formula>>> deps;
	for (auto& dep : mdata->pre_dependencies)
		deps.push_back(make_pair(dep.identifier, dep.version_formula));

	return deps;
}

const std::vector<std::string> &InstalledPackageVersion::get_files()
{
	return file_paths;
}

const std::vector<std::string> &InstalledPackageVersion::get_directories()
{
	return directory_paths;
}

shared_ptr<PackageMetaData> InstalledPackageVersion::get_mdata()
{
	return mdata;
}

bool InstalledPackageVersion::installed_automatically() const
{
	return mdata->installation_reason == INSTALLATION_REASON_AUTO;
}


ComputeInstallationGraphResult compute_installation_graph(
		shared_ptr<Parameters> params,
		vector<shared_ptr<PackageMetaData>> installed_packages,
		PackageDB& pkgdb,
		shared_ptr<PackageProvider> pprov,
		const vector<selected_package_t> &selected_packages,
		bool upgrade_mode)
{
	/* Build a map of InstalledPackageVersions from installed_packages */
	map<pair<string, int>, shared_ptr<InstalledPackageVersion>> installed_map;
	for (auto mdata : installed_packages)
	{
		installed_map.insert(make_pair(
					make_pair(mdata->name, mdata->architecture),
					make_shared<InstalledPackageVersion>(mdata, pkgdb)));
	}

	/* Callbacks to interface with the solver */
	auto list_package_versions = [pprov, &installed_map](const string& name, int arch) {
		auto s = pprov->list_package_versions(name, arch);

		/* Add installed version number */
		auto installed = installed_map.find({name, arch});
		if (installed != installed_map.end())
			s.insert(installed->second->get_binary_version());

		return move(vector<VersionNumber>(move_iterator(s.begin()), move_iterator(s.end())));
	};

	auto get_package_version = [pprov, &installed_map]
			(const string &name, int architecture, const VersionNumber &version) {
		auto installed = installed_map.find({name, architecture});
		if (installed != installed_map.end() && installed->second->get_binary_version() == version)
			return static_pointer_cast<PackageVersion>(installed->second);
		else
			return static_pointer_cast<PackageVersion>(
					pprov->get_package(name, architecture, version));
	};

	/* Create solver */
	auto solver = dynamic_pointer_cast<Depres2Solver>(create_solver("depres2"));

	/* Set solver parameters */
	vector<pair<shared_ptr<PackageVersion>, bool>> adapted_installed_packages;
	for (auto& [id, pkg] : installed_map)
		adapted_installed_packages.push_back(make_pair(pkg, pkg->installed_automatically()));

	solver->set_parameters(
			adapted_installed_packages,
			selected_packages,
			list_package_versions,
			get_package_version);

	/* Depres 2 specific policy */
	// solver->set_policy(prefer_upgrade ? Policy::upgrade : Policy::keep_newer);
	solver->set_policy(Policy::upgrade);
	solver->set_evaluate_all(upgrade_mode && selected_packages.empty());

	/* Try to solve the update problem and returnt he result. */
	if (solver->solve())
	{
		return ComputeInstallationGraphResult(move(solver->get_G()));
	}
	else
	{
		string msg;
		for (const auto& error : solver->get_errors())
			msg += (msg.size() ? "\n" : "") + error;

		return ComputeInstallationGraphResult(msg);
	}
}


/* To serialize an installation graph. */
void visit (contracted_ig_node H[], vector<IGNode*>& serialized, int v)
{
	/* There are a more sophisticated approaches to installing one SCC ...
	 */
	for (IGNode *ig_node : H[v].original_nodes)
		serialized.push_back (ig_node);

	for (int w : H[v].children)
	{
		H[w].unvisited_parents.erase (v);
		
		if (H[w].unvisited_parents.size() == 0)
			visit (H, serialized, w);
	}
}

/* If @param pre_deps is set, pre-dependencies are used instead of dependencies.
 * */
vector<IGNode*> serialize_igraph (
		const installation_graph_t& igraph, bool pre_deps)
{
	/* Construct a data structure to hold the nodes' working variables and
	 * adjacency lists. */
	int cnt_nodes = igraph.size();
	unique_ptr<scc_node[]> nodes = unique_ptr<scc_node[]> (new scc_node[cnt_nodes]);

	auto iter = igraph.begin();

	for (int i = 0; i < cnt_nodes; i++)
	{
		nodes[i].data = (void*) iter->second.get();
		iter->second->algo_priv = i;

		iter++;
	}

	for (int i = 0; i < cnt_nodes; i++)
	{
		auto ig_node = (IGNode*) nodes[i].data;

		if (pre_deps)
		{
			for (IGNode* dep : ig_node->pre_dependencies)
				nodes[i].children.push_back (dep->algo_priv);
		}
		else
		{
			for (IGNode* dep : ig_node->dependencies)
				nodes[i].children.push_back (dep->algo_priv);
		}
	}


	/* Run Tarjan's algorithm */
	int count_sccs = find_scc (cnt_nodes, nodes.get());


	/* Construct the graph with the original graph's SCCs contracted and
	 * traverse it in such a way that a node is output only after all its
	 * ancestors are visited. */
	unique_ptr<contracted_ig_node[]> H = unique_ptr<contracted_ig_node[]> (new contracted_ig_node[count_sccs]);

	/* It's important to transpose the graph here to do the traversal along
	 * edges in forward direction. */
	for (int v = 0; v < cnt_nodes; v++)
	{
		H[nodes[v].SCC].original_nodes.push_back ((IGNode*) nodes[v].data);

		for (int w : nodes[v].children)
		{
			if (nodes[v].SCC != nodes[w].SCC)
			{
				H[nodes[w].SCC].children.insert (nodes[v].SCC);
				H[nodes[v].SCC].unvisited_parents.insert (nodes[w].SCC);
				H[nodes[v].SCC].has_parent = true;
			}
		}
	}

	/* Obviously it's enough to ensure that all parents were visited before. */
	vector<IGNode*> serialized;

	for (int v = 0; v < count_sccs; v++)
	{
		if (!H[v].has_parent)
			visit (H.get(), serialized, v);
	}

	return serialized;
}


vector<shared_ptr<PackageMetaData>> find_packages_to_remove (
		vector<shared_ptr<PackageMetaData>> installed_packages,
		const installation_graph_t& igraph)
{
	vector<shared_ptr<PackageMetaData>> to_remove;

	for (auto pkg : installed_packages)
	{
		auto iv = igraph.find (make_pair (pkg->name, pkg->architecture));
		if (iv != igraph.end())
		{
			if (iv->second->installed_version->get_binary_version() !=
					iv->second->chosen_version->get_binary_version())
			{
				to_remove.push_back (pkg);
			}
		}
		else
		{
			to_remove.push_back (pkg);
		}
	}

	return to_remove;
}

compute_operations_result compute_operations (
		PackageDB& pkgdb,
		installation_graph_t& igraph,
		vector<shared_ptr<PackageMetaData>>& pkgs_to_remove,
		vector<IGNode*>& ig_nodes)
{
	compute_operations_result result;

	/* First, build set B so that the reverse edges can be inserted when forward
	 * edges are inserted. Packages that are part of the final configuration but
	 * are already configured cannot be adjacent to packages from set A, because
	 * otherwise they they would replace a package. However mark them as not
	 * used to be safe. */
	for (auto& p : igraph)
		p.second->algo_priv = -1;

	int i = 0;

	for (auto ig_node : ig_nodes)
	{
		auto instv = dynamic_pointer_cast<InstalledPackageVersion>(ig_node->chosen_version);
		if (ig_node->installed_version &&
				ig_node->installed_version->get_binary_version() == ig_node->chosen_version->get_binary_version() &&
				instv->get_mdata()->state == PKG_STATE_CONFIGURED &&
				ig_node->installed_automatically ==
					(instv->get_mdata()->installation_reason == INSTALLATION_REASON_AUTO))
		{
			continue;
		}

		result.B.emplace_back (-1, ig_node);
		ig_node->algo_priv = i++;
	}

	/* Build a file trie with all package's files */
	FileTrie<vector<PackageMetaData*>> file_trie;

	/* Add packages from installation graph G */
	for (auto& [id, pnode] : igraph)
	{
		for (const auto& file : pnode->chosen_version->get_files())
		{
			auto mdata = dynamic_pointer_cast<InstallationPackageVersion>(
					pnode->chosen_version)->get_mdata();

			file_trie.insert_file(file);
			file_trie.find_file(file)->data.push_back(mdata.get());
		}
	}

	/* Build the bipartite graph */
	for (auto pkg : pkgs_to_remove)
	{
		pkg_operation op (-1, pkg);

		/* Find each package that will conflict with this one and add an edge
		 * for it. */
		for (auto& file : pkgdb.get_files (pkg))
		{
			/* Again, it suffices to use non-directories only to detect
			 * conflicting packages. Note that this will not temporarily remove
			 * directories as old files are removed after new ones are unpacked.
			 * */
			if (file.type != FILE_TYPE_DIRECTORY)
			{
				auto h = file_trie.find_file (file.path);
				if (h)
				{
					/* Will usually be only one */
					for (auto mdata : h->data)
					{
						auto iv = igraph.find (make_pair (mdata->name, mdata->architecture));

						/* Should not happen, but be safe. */
						if (iv == igraph.end() || iv->second->algo_priv == -1)
						{
							throw gp_exception ("INTERNAL ERROR: depress::compute_operations: "
									"Conflict with a file that does not belong "
									"to a package that will be new in the final configuration.");
						}

						shared_ptr<IGNode> v = iv->second;

						if (find (
									op.involved_ig_nodes.begin(),
									op.involved_ig_nodes.end(),
									v.get()) == op.involved_ig_nodes.end())
						{
							op.involved_ig_nodes.push_back (v.get());

							/* Add reverse edge */
							result.B[v->algo_priv].involved_packages.push_back (pkg);
						}
					}
				}
			}
		}

		/* If the package will be upgraded, an edge that indicates a conflict
		 * must be added even if the two versions do not collide as only one
		 * version of a package might be installed in an accepted state (i.e.
		 * configured) at a time. */
		auto new_i = igraph.find (make_pair (pkg->name, pkg->architecture));
		if (new_i != igraph.end())
		{
			auto v = new_i->second;

			/* Should not happen, but be safe. */
			if (v->algo_priv == -1)
			{
				throw gp_exception ("INTERNAL ERROR: depress::compute_operations: "
						"Conflict with a newer version that is not in B.");
			}

			if (find (
						op.involved_ig_nodes.begin(),
						op.involved_ig_nodes.end(),
						v.get()) == op.involved_ig_nodes.end())
			{
				op.involved_ig_nodes.push_back (v.get());

				/* Add reverse edge */
				result.B[v->algo_priv].involved_packages.push_back (pkg);
			}
		}

		result.A.push_back (move (op));
	}

	/* Determine operations */
	for (auto& a : result.A)
	{
		if (a.involved_ig_nodes.empty())
		{
			a.operation = pkg_operation::REMOVE;
		}
		else if (a.involved_ig_nodes.size() > 1)
		{
			a.operation = pkg_operation::REPLACE_REMOVE;
		}
		else
		{
			if (a.involved_ig_nodes[0]->chosen_version->get_name() == a.pkg->name &&
					a.involved_ig_nodes[0]->chosen_version->get_architecture() == a.pkg->architecture)
			{
				a.operation = pkg_operation::CHANGE_REMOVE;
			}
			else
			{
				a.operation = pkg_operation::REPLACE_REMOVE;
			}
		}
	}

	for (auto& b : result.B)
	{
		if (b.involved_packages.empty())
		{
			b.operation = pkg_operation::INSTALL_NEW;
		}
		else if (b.involved_packages.size() == 1 &&
				b.involved_packages[0]->name == b.ig_node->chosen_version->get_name() &&
				b.involved_packages[0]->architecture == b.ig_node->chosen_version->get_architecture())
		{
			b.operation = pkg_operation::CHANGE_INSTALL;
		}
		else
		{
			b.operation = pkg_operation::REPLACE_INSTALL;
		}
	}

	return result;
}


vector<pkg_operation> order_operations (compute_operations_result& bigraph, bool pre_deps)
{
	vector<pkg_operation> sequence;

	/* Prepare */
	vector<shared_ptr<PackageMetaData>> pkgs_to_remove;

	int i = 0;
	for (auto& a : bigraph.A)
	{
		a.algo_priv = false;
		pkgs_to_remove.push_back (a.pkg);
		a.pkg->algo_priv = i++;
	}

	/* Build a partial removal graph */
	RemovalGraphBranch rgraph = build_removal_graph (pkgs_to_remove);


	auto current_b = bigraph.B.begin();

	while (current_b != bigraph.B.end())
	{
		if (current_b->operation == pkg_operation::INSTALL_NEW)
		{
			sequence.push_back (*current_b++);
		}
		else
		{
			/* Remove the conflicting packages first, and all the packages that
			 * would depend on them. */
			for (shared_ptr<PackageMetaData> a_pkg : current_b->involved_packages)
			{
				auto& a = bigraph.A[a_pkg->algo_priv];

				if (!a.algo_priv)
				{
					auto remove_subsequence = serialize_rgraph (
							rgraph, pre_deps, a_pkg);

					for (auto rg_node : remove_subsequence)
					{
						auto& op = bigraph.A[rg_node->pkg->algo_priv];

						if (!op.algo_priv)
						{
							sequence.push_back (op);
							op.algo_priv = true;
						}
					}
				}
			}

			sequence.push_back (*current_b++);
		}
	}

	/* Add unassigned removal operations at the end */
	for (auto& a : bigraph.A)
		if (!a.algo_priv)
			sequence.push_back (a);

	return sequence;
}


vector<pkg_operation> generate_installation_order_from_igraph (
		PackageDB& pkgdb,
		installation_graph_t& igraph,
		vector<shared_ptr<PackageMetaData>>& installed_packages,
		bool pre_deps)
{
	auto ig_node_sequence = serialize_igraph (igraph, pre_deps);
	auto pkgs_to_remove = find_packages_to_remove (installed_packages, igraph);
	auto bigraph = compute_operations (pkgdb, igraph, pkgs_to_remove, ig_node_sequence);
	return order_operations (bigraph, pre_deps);
}


/* Tarjan's strongly connected components algorithm
 * @returns the count of SCCs. */
void STRONGCONNECT (scc_node nodes[], vector<int>& working_stack, int& i, int& j, int v)
{
	nodes[v].LOWPT = nodes[v].LOWVINE = nodes[v].NUMBER = i;
	i++;

	nodes[v].ONDFSSTACK = true;

	working_stack.push_back (v);
	nodes[v].ONSTACK = true;


	for (int w : nodes[v].children)
	{
		if (nodes[w].NUMBER == -1)
		{
			/* Tree arc */
			STRONGCONNECT (nodes, working_stack, i, j, w);
			nodes[v].LOWPT = MIN(nodes[v].LOWPT, nodes[w].LOWPT);
			nodes[v].LOWVINE = MIN(nodes[v].LOWVINE, nodes[w].LOWVINE);
		}
		else if (nodes[w].ONDFSSTACK)
		{
			/* Frond */
			nodes[v].LOWPT = MIN(nodes[v].LOWPT, nodes[w].NUMBER);
		}
		else if (nodes[w].NUMBER < nodes[v].NUMBER)
		{
			/* Vine */
			if (nodes[w].ONSTACK)
				nodes[v].LOWVINE = MIN(nodes[v].LOWVINE, nodes[w].NUMBER);
		}
	}

	if (nodes[v].LOWPT == nodes[v].NUMBER && nodes[v].LOWVINE == nodes[v].NUMBER)
	{
		while (working_stack.size() > 0 &&
				nodes[working_stack.back()].NUMBER >= nodes[v].NUMBER)
		{
			nodes[working_stack.back()].SCC = j;
			nodes[working_stack.back()].ONSTACK = false;
			working_stack.pop_back();
		}

		j++;
	}

	nodes[v].ONDFSSTACK = false;
}


int find_scc (int cnt_nodes, scc_node nodes[])
{
	int i = 0;
	int j = 0;

	vector<int> working_stack;

	for (int v = 0; v < cnt_nodes; v++)
	{
		if (nodes[v].NUMBER == -1)
			STRONGCONNECT (nodes, working_stack, i, j, v);
	}

	return j;
}


RemovalGraphBranch build_removal_graph (vector<shared_ptr<PackageMetaData>> installed_packages)
{
	RemovalGraphBranch g;

	/* A temporary map for to resolve dependency descriptions to installed
	 * packages. */
	map<pair<string, int>,shared_ptr<RemovalGraphNode>> id_to_node;

	/* Create nodes */
	for (auto pkg : installed_packages)
	{
		auto node = make_shared<RemovalGraphNode>(pkg);

		g.V.push_back (node);

		id_to_node.insert (make_pair (
					make_pair (pkg->name, pkg->architecture),
					node));
	}

	/* Add dependencies and pre-dependencies */
	for (auto node : g.V)
	{
		auto pkg = node->pkg;

		for (auto& dep : pkg->pre_dependencies)
		{
			auto i = id_to_node.find (dep.identifier);
			if (i != id_to_node.end())
			{
				i->second->pre_provided.push_back (node.get());
			}
		}

		for (auto& dep : pkg->dependencies)
		{
			auto i = id_to_node.find (dep.identifier);
			if (i != id_to_node.end())
			{
				i->second->provided.push_back (node.get());
			}
		}
	}

	return g;
}


void rtbtr_visit_remove (RemovalGraphNode *v)
{
	v->algo_priv = 1;

	for (auto u : v->pre_provided)
	{
		if (u->algo_priv != 1)
			rtbtr_visit_remove (u);
	}

	for (auto u : v->provided)
	{
		if (u->algo_priv != 1)
			rtbtr_visit_remove (u);
	}
}

void rtbtr_visit_autoremove (RemovalGraphNode* v)
{
	v->algo_priv = 2;

	for (auto u : v->dependencies)
	{
		if (u->algo_priv == 0)
			rtbtr_visit_autoremove (u);
	}
}

void reduce_to_branch_to_remove (
		RemovalGraphBranch& branch,
		set<pair<string,int>>& pkg_ids,
		bool autoremove)
{
	/* On the use of algo_priv here:
	 *   * 0: unvisited
	 *   * 1: remove
	 *   * 2: keep */

	/* Set all package's marks to unvisited in case autoremove is requested or
	 * otherwise keep. */
	if (autoremove)
	{
		for (auto v : branch.V)
			v->algo_priv = 0;
	}
	else
	{
		for (auto v : branch.V)
			v->algo_priv = 2;
	}

	/* Mark packages to remove */
	for (auto v : branch.V)
	{
		if (v->algo_priv != 1)
		{
			if (pkg_ids.find (make_pair (v->pkg->name, v->pkg->architecture))
					!= pkg_ids.end())
			{
				rtbtr_visit_remove (v.get());
			}
		}
	}

	/* If autoremove is requested, mark packages to autoremove. */
	if (autoremove)
	{
		/* Transpose the graph */
		for (auto v : branch.V)
		{
			for (auto& u : v->pre_provided)
				u->dependencies.insert (v.get());

			for (auto& u : v->provided)
				u->dependencies.insert (v.get());
		}

		/* Visit manually installed packages that will not be removed to mark
		 * their dependencies with keep. */
		for (auto v : branch.V)
		{
			if (v->algo_priv == 0 &&
					v->pkg->installation_reason == INSTALLATION_REASON_MANUAL)
			{
				rtbtr_visit_autoremove (v.get());
			}
		}

		/* Mark the remaining vertices for automatic removal and delete the
		 * transposed graph. */
		for (auto v : branch.V)
		{
			if (v->algo_priv == 0)
				v->algo_priv = 1;

			v->dependencies.clear();
		}
	}

	/* Remove packages. No references need to be removed as all outgoing edges
	 * lead to targets that will be removed, too, and those will be still in the
	 * graph, too. */
	auto i = branch.V.begin();

	while (i != branch.V.end())
	{
		auto curr = i++;

		if ((*curr)->algo_priv == 2)
			branch.V.erase (curr);
	}
}


/* To serialize a removal graph */
vector<RemovalGraphNode*> serialize_rgraph (
		RemovalGraphBranch& branch, bool pre_deps, shared_ptr<PackageMetaData> start_node)
{
	/* Find SCCs. Large memory should be on the heap. */
	int cnt_nodes = branch.V.size();
	unique_ptr<scc_node[]> nodes = unique_ptr<scc_node[]> (new scc_node[cnt_nodes]);

	auto iter = branch.V.begin();

	for (int i = 0; i < cnt_nodes; i++)
	{
		nodes[i].data = (void*) iter->get();
		(*iter)->algo_priv = i;

		iter++;
	}

	for (auto rg_node : branch.V)
	{
		if (pre_deps)
		{
			for (auto p : rg_node->pre_provided)
				nodes[rg_node->algo_priv].children.push_back (p->algo_priv);
		}
		else
		{
			for (auto p : rg_node->provided)
				nodes[rg_node->algo_priv].children.push_back (p->algo_priv);
		}
	}

	/* Run Tarjan's algorithm */
	int cnt_sccs = find_scc (cnt_nodes, nodes.get());

	/* If a start node is specified, mark all nodes that are reachable from it.
	 * All other nodes must not be part of the contracted graph H. Otherwise
	 * mark all nodes s.t. they are included in H. */
	if (start_node)
	{
		/* First, clear all marks and find the start node */
		RemovalGraphNode* rg_start = nullptr;
		for (auto v : branch.V)
		{
			v->algo_priv = 0;

			if (v->pkg == start_node)
				rg_start = v.get();
		}

		/* Find all descendants using DFS */
		vector<RemovalGraphNode*> stack;
		stack.push_back (rg_start);

		while (stack.size() > 0)
		{
			auto v = stack.back();
			stack.pop_back();

			v->algo_priv = 1;

			for (auto u : v->pre_provided)
			{
				if (!u->algo_priv)
					stack.push_back (u);
			}

			for (auto u : v->provided)
			{
				if (!u->algo_priv)
					stack.push_back (u);
			}
		}
	}
	else
	{
		for (auto v : branch.V)
			v->algo_priv = 1;
	}

	/* Contract the SCCs and transpose the contracted graph on the fly. In the
	 * transposed graph edges can be traversed in forward direction to obtion a
	 * topological order that allows for removing packages not before all
	 * descendents are removed. */
	unique_ptr<contracted_rg_node[]> H = unique_ptr<contracted_rg_node[]> (new contracted_rg_node[cnt_sccs]);

	for (int v = 0; v < cnt_nodes; v++)
	{
		if (!((RemovalGraphNode*) nodes[v].data)->algo_priv)
			continue;

		H[nodes[v].SCC].original_nodes.push_back ((RemovalGraphNode*) nodes[v].data);

		for (int u : nodes[v].children)
		{
			if (nodes[v].SCC != nodes[u].SCC)
			{
				H[nodes[u].SCC].children.insert(nodes[v].SCC);
				H[nodes[v].SCC].unvisited_parents.insert (nodes[u].SCC);
			}
		}
	}

	/* Compute a topological order using Kahn's algorithm. */
	vector<RemovalGraphNode*> serialized;
	vector<int> S;

	for (int v = 0; v < cnt_sccs; v++)
	{
		if (H[v].unvisited_parents.empty())
			S.push_back (v);
	}

	/* Kahn's algorithm on the transposed graph */
	while (S.size() > 0)
	{
		auto v = S.back();
		S.pop_back ();

		for (auto rg_node : H[v].original_nodes)
			serialized.push_back (rg_node);

		for (auto u : H[v].children)
		{
			H[u].unvisited_parents.erase (v);

			if (H[u].unvisited_parents.empty())
				S.push_back (u);
		}
	}

	return serialized;
}

}
