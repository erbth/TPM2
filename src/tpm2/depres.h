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
			std::shared_ptr<PackageProvider> pprov,
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
		std::vector<FileTrieNodeHandle<std::vector<PackageMetaData*>>> file_node_handles;


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

		/* The final file trie. It does not contain directories. */
		FileTrie<std::vector<PackageMetaData*>> file_trie;

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


	/* To serialize an installation graph. This does only order the nodes that
	 * shall be installed. It does not consider packages that shall be removed,
	 * and not provide or hold information about why the packages reported shall
	 * be installed. The installation graph should only represent the target
	 * configuration, not how to get there. And this function helps in finding
	 * the latter, but does only one part of the work. */
	std::vector<InstallationGraphNode*> serialize_igraph (
			const InstallationGraph& igraph, bool pre_deps);

	struct contracted_ig_node {
		std::list<InstallationGraphNode*> original_nodes;

		std::set<int> children;
		
		std::set<int> unvisited_parents;
		bool has_parent = false;
	};


	/* Find package versions that shall be removed based on a list of currently
	 * installed packages and an installation graph.  This function is stable as
	 * it does not change the order in which packages are given as input.
	 * pre_deps specifies whether pre- or regular dependencies are to be used.
	 * */
	std::vector<std::shared_ptr<PackageMetaData>> find_packages_to_remove (
			std::vector<std::shared_ptr<PackageMetaData>> installed_packages,
			const InstallationGraph& igraph);


	/* Add information to packages and igraph nodes about why they have to be
	 * installed or removed. */
	struct pkg_operation {
		/* Operations are >= 0 s.t. negative values are left for use by
		 * algorithms. */
		static const char INSTALL_NEW = 0;

		/* The old version must be CHANGE_REMOVE'd, the new one
		 * CHANGE_INSTALL'd. */
		static const char CHANGE_REMOVE = 1;
		static const char CHANGE_INSTALL = 2;

		/* Same for replace. In this case, involved_pkgs will contain a list of
		 * packages that will replace the package if it shall be removed, or a
		 * list of packages that will be replaced by this package. */
		static const char REPLACE_REMOVE = 3;
		static const char REPLACE_INSTALL = 4;

		/* Only remove. Removes i.e. packages that depend on packages that would
		 * conflict but don't conflict themselves. */
		static const char REMOVE = 5;

		char operation;

		/* Removal operations use the PackageMetaData fields, install operations
		 * the InstallationGraphNode* fields. */
		const std::shared_ptr<PackageMetaData> pkg;
		InstallationGraphNode * const ig_node;

		std::vector<std::shared_ptr<PackageMetaData>> involved_packages;
		std::vector<InstallationGraphNode*> involved_ig_nodes;


		/* Like always */
		bool algo_priv;


		pkg_operation (char operation, std::shared_ptr<PackageMetaData> pkg)
			: operation(operation), pkg(pkg), ig_node(nullptr)
		{ }

		pkg_operation (char operation, InstallationGraphNode* ig_node)
			: operation(operation), pkg(nullptr), ig_node(ig_node)
		{ }
	};


	/* This function builds a bipartite graph and uses it do build operations
	 * out of wanted package states. It is stable as it does not change the
	 * order in which installation graph nodes and packages to remove are given.
	 * 
	 * compute_operations does only add operations to the sequence that actually
	 * do something. All others would be skipped by the ll code anyway. */
	struct compute_operations_result {
		/* The bipartite graph represented by adjacency lists in both
		 * directions. The involved_nodes vectors int the operation structurs
		 * are the actual adjacency lists. */
		std::vector<pkg_operation> A;
		std::vector<pkg_operation> B;
	};

	compute_operations_result compute_operations (
			PackageDB& pkgdb,
			InstallationGraph& igraph,
			std::vector<std::shared_ptr<PackageMetaData>>& pkgs_to_remove,
			std::vector<InstallationGraphNode*>& ig_nodes);


	/* Order the operations from compute_operations into one sequence that can
	 * be performed by the ll part. */
	std::vector<pkg_operation> order_operations (compute_operations_result& bigraph, bool pre_deps);


	/* All-in-one function to generate a sequence of operations from an
	 * installation graph. @param pre_deps specifies whether pre-dependencies or
	 * dependencies shall be used for package ordering. Well, this is not
	 * perfect as this way imposes that choice on the entire sequence, which
	 * splits the install/upgrade operation into 4 part: (1) unconfigure, (2)
	 * unpack, (3) rm files, (4) configure. That does not minimize the delay
	 * between unconfigure and configure again, but ensures that package's
	 * dependencies are met. However I feel this could work with a smaller
	 * delay, too. But I think I don't need that by now. The perfect solution
	 * would be that the packages support that delay, anyway. But a small enough
	 * delay (a few pkg ops?) should do, too, if I had it ... Anyway. This is
	 * reality. */
	std::vector<pkg_operation> generate_installation_order_from_igraph (
			PackageDB& pkgdb,
			InstallationGraph& igraph,
			std::vector<std::shared_ptr<PackageMetaData>>& installed_packages,
			bool pre_deps);


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
	RemovalGraphBranch build_removal_graph (
			std::vector<std::shared_ptr<PackageMetaData>> installed_packages);

	/* Sort out a branch of packages that must be removed when remove the given
	 * set of packages. */ 
	void reduce_to_branch_to_remove (
			RemovalGraphBranch& branch,
			std::set<std::pair<std::string, int>>& pkg_ids);

	std::vector<RemovalGraphNode*> serialize_rgraph (
			RemovalGraphBranch& branch,
			bool pre_deps,
			std::shared_ptr<PackageMetaData> start_node = nullptr);

	struct contracted_rg_node
	{
		std::vector<RemovalGraphNode*> original_nodes;

		std::set<int> children;

		std::set<int> unvisited_parents;
		bool has_parent = false;
	};
}

#endif /* __DEPRES_H */
