/** This file is part of the TSClient LEGACY Package Manager
 *
 * Common algorithms and data structures that are used by all solvers (i.e. the
 * installation graph representation), and an interface for solvers of the
 * installation problem. Some people may call it upgrade problem as well.
 *
 * Note that there is a certain amount of coupling between the installation
 * graph defined in this common module and the actual solvers. Essentially, the
 * solvers and installation graph need to work together closely as the latter
 * does all sorts of operations on the former while solving the problem. If this
 * is not desired, another abstraction interface that abstracts this
 * installation graph defined here away, may be needed. In this regard 'depres'
 * may be the name of a class of dependency solvers that originates from the
 * original TPM's depres module. However if such another abstraction layer will
 * ever be required it may be sensible to rename this class and hence the depres
 * interface. */
#ifndef __DEPRES_COMMON_H
#define __DEPRES_COMMON_H

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "package_constraints.h"
#include "package_version.h"


namespace depres
{
	/* Prototypes */
	class IGNode;
	class SolverInterface;

	/* Simple type "definitions" */
	using installation_graph_t = std::map<
		std::pair<const std::string, const int>,
		std::shared_ptr<IGNode>>;

	using selected_package_t = std::pair<
		std::pair<std::string, int>,
		std::shared_ptr<const PackageConstraints::Formula>>;

	using cb_list_package_versions_t = std::function<
		std::vector<VersionNumber>(
				const std::string &name, int architecture)>;

	using cb_get_package_version_t = std::function<
		std::shared_ptr<PackageVersion>(
				const std::string &name, int architecture, const VersionNumber &version)>;

	/* A common installation graph representation that may be useful to all
	 * solvers one may write, but at least provide an unique interface to
	 * consumers of the solvers' results. */
	class IGNode
	{
	public:
		SolverInterface &s;

		/* name, version */
		std::pair<const std::string, const int> identifier;

		/* Constraints */
		/* A map of optional source identifier -> version constraining fomula.
		 * At most one source identifier may be ommited to indicate constraints
		 * imposed by the user. */
		std::map<
				IGNode*,
				std::shared_ptr<const PackageConstraints::Formula>
			> constraints;

		/* Dependencies and pre-dependencies (a shortcut on the graph and not on
		 * packages like the chosen version returns them) */
		std::vector<IGNode*> dependencies;
		std::vector<IGNode*> pre_dependencies;

		/* Reverse dependencies */
		std::set<IGNode*> reverse_dependencies;
		std::set<IGNode*> reverse_pre_dependencies;

		/* Chosen version */
		std::shared_ptr<PackageVersion> chosen_version;

		/* Installed version */
		std::shared_ptr<PackageVersion> installed_version;

		/* If the package is selected by the user, and if not, if it was
		 * installed automatically. */
		bool is_selected;
		bool installed_automatically;

		/* Private data to use by algorithms etc. Must not be relied on to be
         * present after the function that uses them exits, however it is not
         * altered by the graph's members. Only by third entities. */
        ssize_t algo_priv;

		/* A constructor and virtual destructor */
		IGNode(
				SolverInterface& s,
				const std::pair<const std::string, const int> &identifier,
				const bool is_selected,
				const bool installed_automatically);

		virtual ~IGNode();

		std::string identifier_to_string() const;
		std::string get_name() const;
		int get_architecture() const;

		/* Set dependencies and reverse dependencies based on chosen_version. If
		 * the lasster is nullptr, all dependencies and reverse dependencies are
		 * removed. */
		void set_dependencies();

		/* Unset the chosen version. This calls `set_dependencies`, hence that
		 * does not need to be done manually. */
		void unset_chosen_version();

		/** @returns true if the chosen version satisfies all constraints of the
		 * node, false if not. If no version is chosen, false is returned. */
		bool version_is_satisfying();

		/** Unset the chosen version if it does not meet the constraints.
		 * @returns True if the version was unset, false if not. */
		bool unset_unsatisfying_version();
	};

	/* The main interface for package solvers */
	/* Typical workflow: instantiate, set_parameters(...), solve() -> true /
	 * false, get_igraph() / get_errors() */
	class SolverInterface
	{
		friend IGNode;

	protected:
		/* Methods for manipulating the installation graph G. */

		/* Retrieves an existing node or adds a new one with that name and
		 * architecture to the installation graph. In case a new node is added,
		 * it is placed on the active set. */
		virtual std::shared_ptr<IGNode> get_or_add_node(
				const std::pair<const std::string, const int> &identifier) = 0;

	public:
		virtual ~SolverInterface() { };

		/* @param installed_packages  [(version, automatically installed)] */
		virtual void set_parameters(
				const std::vector<std::pair<std::shared_ptr<PackageVersion>, bool>> &installed_packages,
				const std::vector<selected_package_t> &selected_packages,
				cb_list_package_versions_t cb_list_package_versions,
				cb_get_package_version_t cb_get_package_version) = 0;

		/** Actually solve the given instance of the installation problem.
		 * @returns True if solving succeded, false otherwise. */
		virtual bool solve() = 0;

		virtual std::vector<std::string> get_errors() const = 0;

		/* This will move the installation graph. */
		virtual installation_graph_t get_G() = 0;


		/* Enable debug log */
		virtual void enable_debug_log(bool) = 0;
	};

	/* Render the installation graph as string in dot-format. WARNING: This
	 * modifies algo_priv. */
	std::string installation_graph_to_dot(
			installation_graph_t &G, const std::string &name = "G");
}

#endif /* __DEPRES_COMMON_H */
