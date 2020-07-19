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

namespace pc = PackageConstraints;

/* The depres algorithm version 2.0 */
namespace depres2
{
	/** Nodes of the installation graph. */
	class IGNode
	{
	public:
		/* name, version */
		std::pair<const std::string, const int> identifier;

		/* Constraints */
		/* A map of optional source identifier -> version constraining
		 * formula. At most one ource identifier may be omitted to indicate
		 * constraints imposed by the user. */
		std::map<
				std::optional<std::pair<const std::string, const int>>,
				std::shared_ptr<pc::Formula>
			> constraints;

		/* Choosen version */
		std::shared_ptr<PackageVersion> choosen_version;

		/* Installed version */
		std::shared_ptr<PackageVersion> installed_version;

		/* If the package is selected by the user */
		const bool is_selected;

		IGNode(
				const std::pair<const std::string, const int> &identifier,
				const bool is_selected)
			:
				identifier(identifier),
				is_selected(is_selected)
		{
		}
	};

	/* A solver for the update problem. Typical workflow: instantiate, solve()
	 * -> true / false, get_igraph() / retrieve_errors() */
	class Solver
	{
		using list_package_versions_t = std::function<
			const std::vector<const VersionNumber>(
					const std::string &name, int architecture)>;

		using get_package_version_t = std::function<
			std::shared_ptr<PackageVersion>(
					const std::string &name, int architecture, const VersionNumber &version)>;

		using installation_graph_t = std::map<std::pair<const std::string, const int>, IGNode>;

	private:
		std::vector<std::shared_ptr<PackageVersion>> installed_packages;
		std::vector<std::tuple<const std::string, int, const VersionNumber>> selected_packages;
		list_package_versions_t list_package_versions;
		get_package_version_t get_package_version;

		installation_graph_t G;
		std::set<IGNode&> active;

		std::vector<std::string> errors;

		/* Methods for manipulating the installation graph G. */
		IGNode &get_or_add_node(const std::pair<const string, const int> &identifier);

	public:
		Solver(
				std::vector<std::shared_ptr<PackageVersion>> installed_packages,
				std::vector<std::tuple<const std::string, int, const VersionNumber>> selected_packages,
				list_package_versions_t list_package_versions,
				get_package_version_t get_package_version)
			:
				installed_packages(installed_packages),
				selected_packages(selected_packages),
				list_package_versions(list_package_versions),
				get_package_version(get_package_version)
		{
		}

		bool solve();
		std::vector<std::string> retrieve_errors() const;

		/* This will move the installation graph. */
		installation_graph_t get_G();
	};
}

#endif /* __DEPRES2_H */
