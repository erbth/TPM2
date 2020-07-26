/** This file is part of the TSClient LEGACY Package Manager
 *
 * A library for TPM2 that houses functions that are useful to other programs,
 * too, like depres. */
#ifndef __DEPRES2_H
#define __DEPRES2_H

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "package_constraints.h"
#include "package_version.h"


/* The depres algorithm version 2.0 */
namespace depres2
{
	class Solver;

	/** Nodes of the installation graph. */
	class IGNode
	{
	public:
		Solver &s;

		/* name, version */
		std::pair<const std::string, const int> identifier;

		/* Constraints */
		/* A map of optional source identifier -> version constraining
		 * formula. At most one source identifier may be omitted to indicate
		 * constraints imposed by the user. */
		std::map<
				const IGNode*,
				std::shared_ptr<PackageConstraints::Formula>
			> constraints;

		/* Dependencies (a shortcut on the graph and not on packages like the
		 * chosen version returns them) */
		std::vector<IGNode*> dependencies;

		/* Reverse dependencies */
		std::set<IGNode*> reverse_dependencies;

		/* Chosen version */
		std::shared_ptr<PackageVersion> chosen_version;

		/* Installed version */
		std::shared_ptr<PackageVersion> installed_version;

		/* If the package is selected by the user */
		bool is_selected;

		IGNode(
				Solver& s,
				const std::pair<const std::string, const int> &identifier,
				const bool is_selected)
			:
				s(s),
				identifier(identifier),
				is_selected(is_selected)
		{
		}

		/* Set dependencies and reverse dependencies based on chosen_version. If
		 * the latter is nullptr, all dependencies and reverse dependencies are
		 * removed. */
		void set_dependencies();

		/* Unset the chosen version. This calls `set_dependencies`. */
		void unset_chosen_version();

		/* Unset the chosen version if it does not meet the constraints. */
		void unset_unsatisfying_version();

		inline bool operator<(const IGNode& o) const
		{
			return this < &o;
		}
	};

	/* A solver for the update problem. Typical workflow: instantiate, solve()
	 * -> true / false, get_igraph() / retrieve_errors() */
	class Solver
	{
	protected:
		friend IGNode;

		using list_package_versions_t = std::function<
			const std::vector<const VersionNumber>(
					const std::string &name, int architecture)>;

		using get_package_version_t = std::function<
			std::shared_ptr<PackageVersion>(
					const std::string &name, int architecture, const VersionNumber &version)>;

		using selected_package_t = std::pair<
			std::pair<
				const std::string,
				int>,
			std::shared_ptr<PackageConstraints::Formula>>;

		using installation_graph_t = std::map<std::pair<const std::string, const int>, IGNode>;


		std::vector<std::shared_ptr<PackageVersion>> installed_packages;
		std::vector<selected_package_t> selected_packages;

		list_package_versions_t list_package_versions;
		get_package_version_t get_package_version;

		installation_graph_t G;
		std::set<IGNode*> active;

		std::vector<std::string> errors;

		/* Methods for manipulating the installation graph G. */

		/* Retrieves an existing node or adds a new one with that name and
		 * architecture to the installation graph. In case a new node is added,
		 * it is placed on the active set. */
		IGNode &get_or_add_node(const std::pair<const std::string, const int> &identifier);

	public:
		Solver(
				std::vector<std::shared_ptr<PackageVersion>> installed_packages,
				std::vector<selected_package_t> selected_packages,
				list_package_versions_t list_package_versions,
				get_package_version_t get_package_version)
			:
				installed_packages(installed_packages),
				selected_packages(selected_packages),
				list_package_versions(list_package_versions),
				get_package_version(get_package_version)
		{
		}

		virtual ~Solver() {}

		bool solve();
		std::vector<std::string> retrieve_errors() const;

		/* This will move the installation graph. */
		installation_graph_t get_G();
	};
}

#endif /* __DEPRES2_H */
