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
	 * construct a file trie, which will be altered while depres computes the
	 * target configuration. */
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
		active.insert (node.get());

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

				if (!h)
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
				id_node = g.V.insert (
						make_pair (
							d.identifier,
							make_shared<InstallationGraphNode>(
								d.get_name(), d.get_architecture()
							)
						)
					).first;

				active.insert (id_node->second.get());
			}

			auto d_node = id_node->second;

			/* Add edge */
			node->pre_dependencies.push_back(d_node.get());
			d_node->pre_dependees.insert(node.get());

			/* Add version constraining formula */
			d_node->pre_constraints.insert({node.get(), d.version_formula});
		}

		/* Regular dependencies */
		for (const Dependency& d : p->dependencies)
		{
			auto id_node = g.V.find(d.identifier);

			if (id_node == g.V.end())
			{
				id_node = g.V.insert (
						make_pair (
							d.identifier,
							make_shared<InstallationGraphNode>(
								d.get_name(), d.get_architecture()
							)
						)
					).first;

				active.insert (id_node->second.get());
			}

			auto d_node = id_node->second;

			/* Add edge */
			node->dependencies.push_back(d_node.get());
			d_node->dependees.insert(node.get());

			/* Add version constraining formula */
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
		mdata->state = PKG_STATE_INVALID;

		auto& old_mdata = pnode->chosen_version;

		/* A package can be uniquely identified by it's binary version number */
		if (!old_mdata || old_mdata->version != mdata->version)
		{
			/* Remove old files */
			for (auto& h : pnode->file_node_handles)
			{
				/* If we have entries in the file trie, these must have come
				 * from somewhere. Hence this node must have a chosen version.
				 * */
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
					d->pre_constraints.erase(pnode);
					d->pre_dependees.erase (pnode);
				}

				pnode->pre_dependencies.clear();

				for (auto &d : pnode->dependencies)
				{
					d->constraints.erase(pnode);
					d->dependees.erase (pnode);
				}

				pnode->dependencies.clear();
			}


			/* Add new files */
			for (auto& file : *provpkg->get_files ())
			{
				if (file.type != FILE_TYPE_DIRECTORY)
				{
					FileTrieNodeHandle<vector<PackageMetaData*>> h;
					h = g.file_trie.find_file (file.path);

					if (!h)
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


	/* Remove conflicting packages */
	/* First, build a set of wanted packages */
	set<pair<const string, int>> wanted_packages;
	for (auto& np : new_packages)
		wanted_packages.insert (make_pair (np.first.first, np.first.second));

	/* Actually remove packages */
	for (auto iv = g.V.begin(); iv != g.V.end();)
	{
		auto v = iv->second;
		bool next_v_selected = false;

		for (auto h : v->file_node_handles)
		{
			if (h->data.size() > 1)
			{
				/* Conflict. Find out which package to keep and remove the
				 * others. */
				PackageMetaData* pkg_to_keep = nullptr;

				for (auto mdata : h->data)
				{
					pkg_to_keep = mdata;

					if (
							mdata->state == PKG_STATE_INVALID ||
							mdata->state == PKG_STATE_PREINST_CHANGE ||
							mdata->state == PKG_STATE_UNPACK_CHANGE ||
							mdata->state == PKG_STATE_WAIT_OLD_REMOVED ||
							mdata->state == PKG_STATE_CONFIGURE_CHANGE
							)
					{
						break;
					}
				}

				/* The vector will be altered. Hence we need a copy of it. */
				vector<PackageMetaData*> to_remove;
				to_remove.reserve (h->data.size());

				for (auto mdata : h->data)
					if (mdata != pkg_to_keep)
						to_remove.push_back (mdata);

				/* Remove the packages that shall not be kept. */
				auto current_node_key = iv->first;

				for (auto mdata : to_remove)
				{
					auto res = cig_remove_node (g, mdata, wanted_packages);
					if (!res.success)
					{
						return ComputeInstallationGraphResult (
								ComputeInstallationGraphResult::NOT_FULFILLABLE,
								res.error);
					}
				}

				/* Update the iterator and continue the loop over packages. */
				auto iv2 = g.V.lower_bound (current_node_key);

				if (iv2 != iv)
				{
					next_v_selected = true;
					iv = iv2;
					break;
				}
			}
		}

		if (!next_v_selected)
			++iv;
	}

	return ComputeInstallationGraphResult(g);
}

CIGRemoveNodeResult cig_remove_node (
		InstallationGraph& g,
		PackageMetaData* mdata,
		set<pair<const string, int>> wanted_packages)
{
	list<InstallationGraphNode*> to_remove;

	to_remove.push_back (g.V.find (make_pair (mdata->name, mdata->architecture))->second.get());

	for (auto jv = to_remove.begin(); jv != to_remove.end();)
	{
		auto v = *jv;

		/* Check if the node is allowed to be removed and if yes, remove it.
		 * Otherwise return an error message. */
		if (wanted_packages.find (make_pair (v->name, v->architecture)) != wanted_packages.end())
		{
			return CIGRemoveNodeResult ("Invalid configuration: "
					+ v->name + "@" + Architecture::to_string (v->architecture) +
					" cannot be installed due to conflicts.");
		}

		/* Add the dependees to the list of packages to remove. Their backwards
		 * edges can stay in place as the whole node will be removed before this
		 * function returns and this function does not use the
		 * installation-graph-edges. */
		for (auto u : v->pre_dependees)
			to_remove.push_back (u);

		for (auto u : v->dependees)
			to_remove.push_back (u);

		/* Remove the node's files */
		for (auto& h : v->file_node_handles)
		{
			/* If we have entries in the file trie, these must have come
			 * from somewhere. Hence this node must have a chosen version.
			 * */
			auto i = find (h->data.begin(), h->data.end(), v->chosen_version.get());
			h->data.erase (i);

			if (h->data.empty())
				g.file_trie.remove_element (h->get_path());
		}

		/* Remove the node from the graph */
		g.V.erase (make_pair (v->name, v->architecture));

		/* Process the next node */
		to_remove.erase (jv++);
	}

	return CIGRemoveNodeResult();
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

	/* Construct the transposed of the transposed contracted graph, thus the
	 * 'normal' graph. First, it's important to transpose it such that
	 * traversing a forward edge goes to a package that can be removed next. To
	 * compute a topological order with the used algorithm the graph needs to be
	 * reversed again. */
	unique_ptr<contracted_rg_node[]> H = unique_ptr<contracted_rg_node[]> (new contracted_rg_node[cnt_sccs]);

	for (int v = 0; v < cnt_nodes; v++)
	{
		H[nodes[v].SCC].original_nodes.push_back ((RemovalGraphNode*) nodes[v].data);

		for (int u : nodes[v].children)
		{
			if (nodes[v].SCC != nodes[u].SCC)
			{
				H[nodes[v].SCC].children.insert(nodes[u].SCC);
				H[nodes[u].SCC].unvisited_parents.insert (nodes[v].SCC);
			}
		}
	}

	/* Traverse the contracted graph using a version of Kahn's algorithm for the
	 * transposed graph. */
	vector<RemovalGraphNode*> serialized;
	vector<int> S;

	/* If a start node is specified, start traversal only there. That does
	 * essentially not cover its ancestors (except if they are descendants, too,
	 * i.e. the start node is part of a cycle). */
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
			S.push_back (v);
	}
	else
	{
		for (int v = 0; v < cnt_sccs; v++)
		{
			if (H[v].unvisited_parents.empty())
				S.push_back (v);
		}
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

	reverse (serialized.begin(), serialized.end());

	return serialized;
}

}
