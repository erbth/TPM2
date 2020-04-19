#include <algorithm>
#include <set>
#include "depres.h"
#include "architecture.h"
#include "common_utilities.h"

using namespace std;


namespace depres
{

ComputeInstallationGraphResult compute_installation_graph(
		shared_ptr<Parameters> params,
		vector<shared_ptr<PackageMetaData>> installed_packages,
		PackageDB& pkgdb,
		shared_ptr<PackageProvider> pprov,
		vector<
			pair<
				pair<const string, int>,
				shared_ptr<const PackageConstraints::Formula>
			>
		> new_packages)
{
	/* The installation graph with a file trie to efficientry test for
	 * conflicting packages. */
	InstallationGraph g;

	/* A set of active packages */
	set<InstallationGraphNode*> active;

	/* Add all installed packages to the installation graph. This does also
	 * construct two file tries, one that will be altered while depres computes
	 * the target configuration and one that will represent the original
	 * situation for later use while packages are installed. */
	for (auto p : installed_packages)
	{
		auto node = make_shared<InstallationGraphNode>(
				p->name,
				p->architecture,
				true,
				p);

		node->sms = make_shared<StoredMaintainerScripts> (params, p);
		node->sms->clear_buffers();

		g.add_node(node);

		auto files = pkgdb.get_files (p);
		for (auto& file : files)
		{
			/* For package collision detection it's enough to consider files
			 * only. Directories maybe present in many packages, and files are
			 * typically not. The latter allows for simpler file trie node to
			 * package pointers. */
			if (file.type != FILE_TYPE_DIRECTORY)
			{
				FileTrieNodeHandle<vector<PackageMetaData*>> h;
				h = g.file_trie.find_file (file.path);

				if (h)
				{
					return ComputeInstallationGraphResult(
							ComputeInstallationGraphResult::INVALID_CURRENT_CONFIG,
							"Conflicting package " + p->name + "@" +
							Architecture::to_string(p->architecture) +
							" currently installed.");
				}
				else
				{
					g.file_trie.insert_file (file.path);
					h = g.file_trie.find_file (file.path);
				}

				/* No point in making this faster as this vector will only have
				 * two elements in an valid configuration. */
				h->data.push_back (p.get());

				node->file_node_handles.push_back (h);
			}
		}
	}

	/* Add dependencies of installed packages to the installation graph */
	for (auto p : installed_packages)
	{
		auto node = g.V.find({p->name, p->architecture})->second;

		/* Pre-dependencies */
		for (const Dependency& d : p->pre_dependencies)
		{
			auto id_node = g.V.find(d.identifier);

			if (id_node == g.V.end())
			{
				return ComputeInstallationGraphResult(
						ComputeInstallationGraphResult::INVALID_CURRENT_CONFIG,
						"Pre-dependency " + d.get_name() + "@" +
						Architecture::to_string(d.get_architecture()) +
						" not installed.");
			}

			auto d_node = id_node->second;

			/* Add edge */
			node->pre_dependencies.push_back(d_node.get());
			d_node->pre_dependees.insert(node.get());

			/* Add version constraining formulae */
			d_node->pre_constraints.insert({node.get(), d.version_formula});
		}

		/* Regular dependencies */
		for (const Dependency& d : p->dependencies)
		{
			auto id_node = g.V.find(d.identifier);

			if (id_node == g.V.end())
			{
				return ComputeInstallationGraphResult(
						ComputeInstallationGraphResult::INVALID_CURRENT_CONFIG,
						"Dependency " + d.get_name() + "@" +
						Architecture::to_string(d.get_architecture()) +
						" not installed.");
			}

			auto d_node = id_node->second;

			/* Add edge */
			node->dependencies.push_back(d_node.get());
			d_node->dependees.insert(node.get());

			/* Add version constraining formulae */
			d_node->constraints.insert({node.get(), d.version_formula});
		}
	}


	/* Maybe check if the current configuration is valid. That is, apart from
	 * what has already been done above, the chosen versions must fulfill the
	 * constraints of dependencies and the installed packages must not conflict.
	 * */


	/* Add new packages */
	for (auto& new_pkg : new_packages)
	{
		shared_ptr<InstallationGraphNode> node;

		auto p = g.V.find (new_pkg.first);

		bool was_new = false;

		if (p != g.V.end())
		{
			node = p->second;
		}
		else
		{
			was_new = true;

			node = make_shared<InstallationGraphNode>(
					new_pkg.first.first, new_pkg.first.second);
		}

		node->manual = true;

		if (new_pkg.second)
			node->constraints.insert ({nullptr, new_pkg.second});

		if (was_new)
			g.add_node (node);

		active.insert (node.get());
	}


	/* Choose versions and resolve dependencies */
	while (active.size() > 0)
	{
		auto i = active.begin();
		auto pnode = *i;
		active.erase (i);

		set<VersionNumber> vers = pprov->list_package_versions (
				pnode->name, pnode->architecture);

		if (vers.size() == 0)
		{
			return ComputeInstallationGraphResult (
					ComputeInstallationGraphResult::NOT_FULFILLABLE,
					"No version of package " + pnode->name + "@" +
					Architecture::to_string (pnode->architecture) + " available.");
		}

		/* Rule out versions that don't fulfill the constraints and choose the
		 * newest one that does (newest according to binary version, there is
		 * always one). */
		shared_ptr<ProvidedPackage> provpkg;

		for (auto i = vers.rbegin(); !provpkg && i != vers.rend(); i++)
		{
			provpkg = pprov->get_package (
					pnode->name, pnode->architecture, *i);

			if (!provpkg)
			{
				return ComputeInstallationGraphResult (
						ComputeInstallationGraphResult::SOURCE_ERROR,
						"Could not fetch package " + pnode->name + "@" +
						Architecture::to_string(pnode->architecture) + ":" +
						i->to_string());
			}


			for (auto& p : pnode->pre_constraints)
			{
				if (!p.second->fulfilled (
							provpkg->get_mdata()->source_version,
							provpkg->get_mdata()->version))
				{
					provpkg = nullptr;
					break;
				}
			}

			if (provpkg)
			{
				for (auto& p : pnode->constraints)
				{
					if (!p.second->fulfilled (
								provpkg->get_mdata()->source_version,
								provpkg->get_mdata()->version))
					{
						provpkg = nullptr;
						break;
					}
				}
			}
		}

		if (!provpkg)
		{
			return ComputeInstallationGraphResult (
					ComputeInstallationGraphResult::NOT_FULFILLABLE,
					"No available version of package " + pnode->name + "@" +
					Architecture::to_string (pnode->architecture) +
					" fulfills all version requirements.");
		}


		/* Only change something if it is a different package */
		auto mdata = provpkg->get_mdata();
		auto& old_mdata = pnode->chosen_version;

		/* A package can be uniquely identified by it's binary version number */
		if (!old_mdata || old_mdata->version != mdata->version)
		{
			/* Remove old files */
			for (auto& h : pnode->file_node_handles)
			{
				/* If we have entries in the file trie that mus havae come from
				 * somewhere. Hence this node must have a chosen version. */
				auto i = find (h->data.begin(), h->data.end(), old_mdata.get());
				h->data.erase (i);

				if (h->data.empty())
					g.file_trie.remove_element (h->get_path());
			}

			pnode->file_node_handles.clear();


			/* Remove old dependencies and constraints */
			if (old_mdata)
			{
				for (auto &d : pnode->pre_dependencies)
				{
					d->pre_constraints.erase(d);
					d->pre_dependees.erase (d);
				}

				pnode->pre_dependencies.clear();

				for (auto &d : pnode->dependencies)
				{
					d->constraints.erase(d);
					d->dependees.erase (d);
				}

				pnode->dependencies.clear();
			}


			/* Check if the new version would conflict. I do this after choosing
			 * a version to make the choice independent from that; i.e. to not
			 * install an older version just because it does not conflict
			 * (usually not what the user wants and if so, she can specify this
			 * manually).
			 *
			 * For now, abort when a conflicting package would be installed. */
			for (auto& file : *provpkg->get_files ())
			{
				if (file.type != FILE_TYPE_DIRECTORY)
				{
					FileTrieNodeHandle<vector<PackageMetaData*>> h;
					h = g.file_trie.find_file (file.path);

					if (h)
					{
						return ComputeInstallationGraphResult (
								ComputeInstallationGraphResult::NOT_FULFILLABLE,
								"Package " + mdata->name + "@" +
								Architecture::to_string (mdata->architecture) + ":" +
								mdata->version.to_string() + " would conflict with other packages.");
					}
					else
					{
						g.file_trie.insert_file (file.path);
						h = g.file_trie.find_file (file.path);
					}

					h->data.push_back (mdata.get());

					pnode->file_node_handles.push_back (h);
				}
			}


			/* Update node */
			pnode->chosen_version = mdata;
			pnode->provided_package = provpkg;

			pnode->chosen_version->installation_reason =
				pnode->manual ? INSTALLATION_REASON_MANUAL : INSTALLATION_REASON_AUTO;


			/* Add new dependencies and constraints */
			for (auto& d : mdata->pre_dependencies)
			{
				InstallationGraphNode *pd_node;

				auto i = g.V.find (d.identifier);
				if (i == g.V.end())
				{
					auto tmp = make_shared<InstallationGraphNode> (
							d.get_name(), d.get_architecture());

					g.V.insert (make_pair (d.identifier, tmp));
					pd_node = tmp.get();
				}
				else
				{
					pd_node = i->second.get();
				}

				pnode->pre_dependencies.push_back (pd_node);
				pd_node->pre_dependees.insert (pnode);
				pd_node->pre_constraints.insert (make_pair (pd_node, d.version_formula));

				/* The dependency must become active again. */
				active.insert (pd_node);
			}

			for (auto& d : mdata->dependencies)
			{
				InstallationGraphNode *pd_node;

				auto i = g.V.find (d.identifier);
				if (i == g.V.end())
				{
					auto tmp = make_shared<InstallationGraphNode> (
							d.get_name(), d.get_architecture());

					g.V.insert (make_pair (d.identifier, tmp));
					pd_node = tmp.get();
				}
				else
				{
					pd_node = i->second.get();
				}

				pnode->dependencies.push_back (pd_node);
				pd_node->dependees.insert (pnode);
				pd_node->constraints.insert (make_pair (pd_node, d.version_formula));

				/* The dependency must become active again. */
				active.insert (pd_node);
			}
		}
	}

	return ComputeInstallationGraphResult(g);
}


InstallationGraphNode::InstallationGraphNode(
		const string &name, const int architecture)
	:
		name(name), architecture(architecture)
{
}

InstallationGraphNode::InstallationGraphNode(
		const string &name, const int architecture, bool this_is_installed,
		shared_ptr<PackageMetaData> chosen_version)
	:
		name(name), architecture(architecture),
		chosen_version(chosen_version),
		currently_installed_version(this_is_installed ? optional(chosen_version->version) : nullopt)
{
	if (this_is_installed && chosen_version->installation_reason == INSTALLATION_REASON_MANUAL)
		manual = true;
}


void InstallationGraph::add_node(shared_ptr<InstallationGraphNode> n)
{
	V.insert({{n->name, n->architecture}, n});
}


/* To serialize an installation graph. */
void visit (contracted_ig_node H[], vector<InstallationGraphNode*>& serialized, int v)
{
	/* There are a more sophisticated approaches to installing one SCC ...
	 */
	for (InstallationGraphNode *ig_node : H[v].original_nodes)
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
vector<InstallationGraphNode*> serialize_igraph (
		const InstallationGraph& igraph, bool pre_deps)
{
	/* Construct a data structure to hold the nodes' working variables and
	 * adjacency lists. */
	int cnt_nodes = igraph.V.size();
	unique_ptr<scc_node[]> nodes = unique_ptr<scc_node[]> (new scc_node[cnt_nodes]);

	auto iter = igraph.V.begin();

	for (int i = 0; i < cnt_nodes; i++)
	{
		nodes[i].data = (void*) iter->second.get();
		iter->second->algo_priv = i;

		iter++;
	}

	for (int i = 0; i < cnt_nodes; i++)
	{
		auto ig_node = (InstallationGraphNode*) nodes[i].data;

		if (pre_deps)
		{
			for (InstallationGraphNode* dep : ig_node->pre_dependencies)
				nodes[i].children.push_back (dep->algo_priv);
		}
		else
		{
			for (InstallationGraphNode* dep : ig_node->dependencies)
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
		H[nodes[v].SCC].original_nodes.push_back ((InstallationGraphNode*) nodes[v].data);

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
	vector<InstallationGraphNode*> serialized;

	for (int v = 0; v < count_sccs; v++)
	{
		if (!H[v].has_parent)
			visit (H.get(), serialized, v);
	}

	return serialized;
}


vector<shared_ptr<PackageMetaData>> find_packages_to_remove (
		vector<shared_ptr<PackageMetaData>> installed_packages,
		const InstallationGraph& igraph)
{
	vector<shared_ptr<PackageMetaData>> to_remove;

	for (auto pkg : installed_packages)
	{
		auto iv = igraph.V.find (make_pair (pkg->name, pkg->architecture));
		if (iv != igraph.V.end())
		{
			if (iv->second->currently_installed_version != iv->second->chosen_version->version)
				to_remove.push_back (pkg);
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
		InstallationGraph& igraph,
		vector<shared_ptr<PackageMetaData>>& pkgs_to_remove,
		vector<InstallationGraphNode*>& ig_nodes)
{
	compute_operations_result result;

	/* First, build set B so that the reverse edges can be inserted when forward
	 * edges are inserted. Packages that are part of the final configuration but
	 * are already configured cannot be adjacent to packages from set A, because
	 * otherwise they they would replace a package. However mark them as not
	 * used to be safe. */
	for (auto& p : igraph.V)
		p.second->algo_priv = -1;

	int i = 0;

	for (auto ig_node : ig_nodes)
	{
		if (ig_node->currently_installed_version &&
				ig_node->currently_installed_version == ig_node->chosen_version->version &&
				ig_node->chosen_version->state == PKG_STATE_CONFIGURED)
		{
			continue;
		}

		result.B.emplace_back (-1, ig_node);
		ig_node->algo_priv = i++;
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
				auto h = igraph.file_trie.find_file (file.path);
				if (h)
				{
					/* Will usually be only one */
					for (auto mdata : h->data)
					{
						auto iv = igraph.V.find (make_pair (mdata->name, mdata->architecture));

						/* Should not happen, but be safe. */
						if (iv == igraph.V.end() || iv->second->algo_priv == -1)
						{
							throw gp_exception ("INTERNAL ERROR: depress::compute_operations: "
									"Conflict with a file that does not belong "
									"to a package that will be new in the final configuration.");
						}

						shared_ptr<InstallationGraphNode> v = iv->second;

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
			if (a.involved_ig_nodes[0]->chosen_version->name == a.pkg->name &&
					a.involved_ig_nodes[0]->chosen_version->architecture == a.pkg->architecture)
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
				b.involved_packages[0]->name == b.ig_node->chosen_version->name &&
				b.involved_packages[0]->architecture == b.ig_node->chosen_version->architecture)
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
		InstallationGraph& igraph,
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


void rtbtr_mark_node (RemovalGraphNode *node)
{
	if (node->algo_priv == 0)
	{
		node->algo_priv = 1;

		for (auto p : node->pre_provided)
			rtbtr_mark_node (p);

		for (auto p : node->provided)
			rtbtr_mark_node (p);
	}
}

void reduce_to_branch_to_remove (RemovalGraphBranch& branch, set<pair<string,int>>& pkg_ids)
{
	/* Clear all package's marks */
	for (auto node : branch.V)
		node->algo_priv = 0;

	/* Mark packages to remove */
	for (auto node : branch.V)
	{
		if (pkg_ids.find (make_pair (node->pkg->name, node->pkg->architecture))
				!= pkg_ids.end())
		{
			rtbtr_mark_node (node.get());
		}
	}

	/* Remove packages. No references need to be removed as all outgoing edges
	 * lead to targets that need to be removed, too, and are hence still in the
	 * graph. */
	auto i = branch.V.begin();

	while (i != branch.V.end())
	{
		auto next = i;

		/* Must be done before i is invalidated. */
		next++;

		if ((*i)->algo_priv == 0)
			branch.V.erase (i);

		i = next;
	}
}


/* To serialize a removal graph */
void visit (contracted_rg_node H[], vector<RemovalGraphNode*>& serialized, int v)
{
	/* There are a more sophisticated approaches for removing one SCC ...
	 */
	for (RemovalGraphNode *rg_node : H[v].original_nodes)
		serialized.push_back (rg_node);

	for (int w : H[v].children)
	{
		H[w].unvisited_parents.erase (v);
		
		if (H[w].unvisited_parents.size() == 0)
			visit (H, serialized, w);
	}
}


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

	/* Construct the transposed contracted graph. It's important to transpose it
	 * such that traversing a forward edge goes to a package that can be removed
	 * next. */
	unique_ptr<contracted_rg_node[]> H = unique_ptr<contracted_rg_node[]> (new contracted_rg_node[cnt_sccs]);

	for (int v = 0; v < cnt_nodes; v++)
	{
		H[nodes[v].SCC].original_nodes.push_back ((RemovalGraphNode*) nodes[v].data);

		for (int u : nodes[v].children)
		{
			if (nodes[v].SCC != nodes[u].SCC)
			{
				H[nodes[u].SCC].children.insert(nodes[v].SCC);
				H[nodes[v].SCC].unvisited_parents.insert (nodes[u].SCC);
				H[nodes[v].SCC].has_parent = true;
			}
		}
	}

	/* Traverse the contracted graph */
	vector<RemovalGraphNode*> serialized;

	/* If a start node is specified, start traversal only there. That does
	 * essentially not cover its ancestors (except if they are descendants, too,
	 * i.e. it is part of a cycle). */
	if (start_node)
	{
		int v = 0;
		bool found = false;

		for (; v < cnt_sccs; v++)
		{
			for (auto rg_node : H[v].original_nodes)
			{
				if (rg_node->pkg == start_node)
				{
					found = true;
					break;
				}
			}

			if (found)
				break;
		}

		if (found)
			visit (H.get(), serialized, v);
	}
	else
	{
		for (int v = 0; v < cnt_sccs; v++)
		{
			if (!H[v].has_parent)
				visit (H.get(), serialized, v);
		}
	}

	return serialized;
}

}
