/** This file is part of the TSClient LEGACY package manager
 *
 * This module contains the depres algorithm and associated data structures. */

#ifndef __DEPRES_H
#define __DEPRES_H

#include <list>
#include <memory>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "package_meta_data.h"
#include "dependencies.h"
#include "package_constraints.h"
#include "package_provider.h"
#include "stored_maintainer_scripts.h"
#include "file_trie.h"
#include "package_db.h"


namespace depres
{
	/* Prototypes */
	struct ComputeInstallationGraphResult;


	ComputeInstallationGraphResult compute_installation_graph(
			std::shared_ptr<Parameters> params,
			std::vector<std::shared_ptr<PackageMetaData>> installed_packages,
			PackageDB& pkgdb,
			std::vector<
				std::pair<
					std::pair<const std::string, int>,
					std::shared_ptr<const PackageConstraints::Formula>
				>
			> new_packages);


	struct InstallationGraphNode
	{
		std::string name;
		int architecture;
		std::shared_ptr<PackageMetaData> chosen_version;

		std::optional<VersionNumber> currently_installed_version;

		/* One of these two must be provided ... */
		std::shared_ptr<ProvidedPackage> provided_package;
		std::shared_ptr<StoredMaintainerScripts> sms;

		bool manual = false;


		/* These formulae are joined conjunctively. In the runtime constraints
		 * may be one element with nullptr as key, which encodes a user
		 * specifies constraint. */
		std::map<InstallationGraphNode*,
			std::shared_ptr<const PackageConstraints::Formula>> pre_constraints;

		std::map<InstallationGraphNode*,
			std::shared_ptr<const PackageConstraints::Formula>> constraints;


		/* To form the graph */
		std::list<InstallationGraphNode*> pre_dependencies;
		std::set<InstallationGraphNode*> pre_dependees;

		std::list<InstallationGraphNode*> dependencies;
		std::set<InstallationGraphNode*> dependees;


		/* A list of file trie nodes associated with the package for easier
		 * removal (contains less storage than a list of filepaths and is faster
		 * than traversing the file trie) . */
		std::vector<FileTrieNodeHandle<std::set<PackageMetaData*>>> file_node_handles;


		/* Private data to use by algorithms etc. Must not be relied on to be
		 * present after the function that uses them exits, however it is not
		 * altered by the graph's members. Only by third entities. */
		ssize_t algo_priv;


		/* Constructors */
		InstallationGraphNode(const std::string &name, const int architecture);

		/* @param this_is_installed true means installed version + manual will
		 *     be set automatically from @param chosen_version. */
		InstallationGraphNode(const std::string &name, const int architecture,
				bool this_is_installed,
				std::shared_ptr<PackageMetaData> chosen_version);
	};

	struct InstallationGraph
	{
		/* A map (name, architecture) -> InstallationGraphNode to easily find
		 * nodes for adding edges. */
		std::map<std::pair<std::string, int>, std::shared_ptr<InstallationGraphNode>> V;

		void add_node(std::shared_ptr<InstallationGraphNode> n);
	};


	struct ComputeInstallationGraphResult
	{
		static const int SUCCESS = 0;
		static const int INVALID_CURRENT_CONFIG = 1;
		static const int NOT_FULFILLABLE = 2;
		static const int SOURCE_ERROR = 3;

		const int status;
		const std::string error_message;

		InstallationGraph g;

		/* Constructors */
		ComputeInstallationGraphResult(int status, const std::string &msg)
			: status(status), error_message(msg)
		{}

		ComputeInstallationGraphResult(InstallationGraph g)
			: status(SUCCESS), g(g)
		{}
	};


	/* To serialize an installation graph */
	std::list<InstallationGraphNode*> serialize_igraph (
			const InstallationGraph& igraph, bool pre_deps);

	struct contracted_ig_node {
		std::list<InstallationGraphNode*> original_nodes;

		std::set<int> children;
		
		std::set<int> unvisited_parents;
		bool has_parent = false;
	};

	/* Tarjan's strongly connected components algorithm */
	struct scc_node {
		void *data = nullptr;

		std::list<int> children;

		int NUMBER = -1;
		int LOWPT = 0;
		int LOWVINE = 0;

		bool ONDFSSTACK = false;
		bool ONSTACK = false;

		int SCC = 0;
	};

	/* @returns the number of SCCs found. */
	int find_scc (int cnt_nodes, scc_node nodes[]);


	/* A graph for removing packages - the Removal Graph. Like the installation
	 * graph, it's stored using adjacency lists. For the removal graph it is not
	 * important in which state a package is. The only thing that matters is
	 * that it's part of the system. */
	struct RemovalGraphNode
	{
		std::shared_ptr<PackageMetaData> pkg;

		/* Reverse dependencies, so the Removal Graph is actualy the transposed
		 * installation graph made of all packages in the system. Another name
		 * for the packages 'provided' by a package would be 'dependees'. But I
		 * did not like that term ... */
		std::list<RemovalGraphNode*> pre_provided;
		std::list<RemovalGraphNode*> provided;

		/* Private data to use by algorithms etc. Must not be relied on to be
		 * present after the function that uses them exits, however it is not
		 * altered by the graph's members. Only by third entities. */
		ssize_t algo_priv;

		RemovalGraphNode (std::shared_ptr<PackageMetaData> pkg)
			: pkg(pkg)
		{ }
	};

	/* This struct can hold either the entire removal graph or just a branch of
	 * it. */
	struct RemovalGraphBranch
	{
		std::list<std::shared_ptr<RemovalGraphNode>> V;
	};

	/* Build the entire removal graph */
	RemovalGraphBranch build_removal_graph (PackageDB& pkgdb);

	/* Sort out a branch of packages that must be removed when remove the given
	 * set of packages. */ 
	void reduce_to_branch_to_remove (
			RemovalGraphBranch& branch,
			std::set<std::pair<std::string, int>>& pkg_ids);

	std::vector<RemovalGraphNode*> serialize_rgraph (
			RemovalGraphBranch& branch,
			bool pre_deps);

	struct contracted_rg_node
	{
		std::vector<RemovalGraphNode*> original_nodes;

		std::set<int> children;

		std::set<int> unvisited_parents;
		bool has_parent = false;
	};
}

#endif /* __DEPRES_H */
