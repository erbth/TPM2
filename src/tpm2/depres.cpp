#include "depres.h"
#include <set>
#include "architecture.h"
#include "common_utilities.h"

using namespace std;


namespace depres
{

ComputeInstallationGraphResult compute_installation_graph(
		shared_ptr<Parameters> params,
		vector<shared_ptr<PackageMetaData>> installed_packages,
		vector<
			pair<
				pair<const string, int>,
				shared_ptr<const PackageConstraints::Formula>
			>
		> new_packages)
{
	/* The installation graph */
	InstallationGraph g;

	/* A set of active packages */
	set<InstallationGraphNode*> active;

	/* Add all installed packages to the installation graph */
	for (auto p : installed_packages)
	{
		g.add_node(make_shared<InstallationGraphNode>(
					p->name,
					p->architecture,
					true,
					p));
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
		auto node = make_shared<InstallationGraphNode>(
				new_pkg.first.first, new_pkg.first.second);

		node->manual = true;

		if (new_pkg.second)
			node->constraints.insert ({nullptr, new_pkg.second});

		g.add_node (node);
		active.insert (node.get());
	}


	/* Choose versions and resolve dependencies */
	shared_ptr<PackageProvider> pprov = PackageProvider::create (params);

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


/* To serialize an installation graph.
 *
 * If @param pre_deps is set, pre-dependencies are used instead of dependencies.
 */
void visit (contracted_node H[], list<InstallationGraphNode*>& serialized, int v)
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

list<InstallationGraphNode*> serialize_igraph (
		const InstallationGraph& igraph, bool pre_deps)
{
	/* Construct a data structure to hold the nodes' working variables and
	 * adjacency lists. */
	int cnt_nodes = igraph.V.size();
	unique_ptr<scc_node[]> nodes = unique_ptr<scc_node[]> (new scc_node[cnt_nodes]);

	auto iter = igraph.V.begin();

	for (int i = 0; i < cnt_nodes; i++)
	{
		nodes[i].igraph_node = iter->second.get();
		nodes[i].igraph_node->algo_priv = i;

		iter++;
	}

	for (int i = 0; i < cnt_nodes; i++)
	{
		if (pre_deps)
		{
			for (InstallationGraphNode* dep : nodes[i].igraph_node->pre_dependencies)
				nodes[i].children.push_back (dep->algo_priv);
		}
		else
		{
			for (InstallationGraphNode* dep : nodes[i].igraph_node->dependencies)
				nodes[i].children.push_back (dep->algo_priv);
		}
	}


	/* Run Tarjan's algorithm */
	int count_sccs = find_scc (cnt_nodes, nodes.get());


	/* Construct the graph with the original graph's SCCs contracted and
	 * traverse it in such a way that a node is output only after all its
	 * ancestors are visited. */
	unique_ptr<contracted_node[]> H = unique_ptr<contracted_node[]> (new contracted_node[count_sccs]);

	/* It's important to transpose the graph here to do the traversal along
	 * edges in forward direction. */
	for (int v = 0; v < cnt_nodes; v++)
	{
		H[nodes[v].SCC].original_nodes.push_back (nodes[v].igraph_node);

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
	list<InstallationGraphNode*> serialized;

	for (int v = 0; v < count_sccs; v++)
	{
		if (!H[v].has_parent)
			visit (H.get(), serialized, v);
	}

	return serialized;
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


}
