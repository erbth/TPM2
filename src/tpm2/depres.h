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


namespace depres
{
	/* Prototypes */
	struct ComputeInstallationGraphResult;


	ComputeInstallationGraphResult compute_installation_graph(
			std::shared_ptr<Parameters> params,
			std::vector<std::shared_ptr<PackageMetaData>> installed_packages,
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

	struct contracted_node {
		std::list<InstallationGraphNode*> original_nodes;

		std::set<int> children;
		
		std::set<int> unvisited_parents;
		bool has_parent = false;
	};

	/* Tarjan's strongly connected components algorithm */
	struct scc_node {
		InstallationGraphNode* igraph_node = nullptr;

		std::list<int> children;

		int NUMBER = -1;
		int LOWPT = 0;
		int LOWVINE = 0;

		bool ONDFSSTACK = false;
		bool ONSTACK = false;

		int SCC = 0;
	};

	int find_scc (int cnt_nodes, scc_node nodes[]);
}

#endif /* __DEPRES_H */
