/** This file is part of the TSClient LEGACY Package Manager
 *
 * Depres version 2
 *
 * */
#ifndef __DEPRES2_H
#define __DEPRES2_H

#include <functional>
#include <map>
#include <memory>
#include <list>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "package_constraints.h"
#include "package_version.h"
#include "depres_common.h"
#include "file_trie.h"


namespace depres
{
	class Depres2Solver;

	/* Custom IGNode subclass with private data */
	class Depres2IGNode : public IGNode
	{
		friend Depres2Solver;

	protected:
		/* Set to true if this node should be removed from the graph (only
		 * required for nodes that cannot be removed immediately / by garbage
		 * collection) */
		bool marked_for_removal = false;

		unsigned t_eject = 0;

		std::list<Depres2IGNode*>::iterator iactive_queue;

		unsigned eject_index = 0;

		/* Clear private data to save memory. To be called when the solver is
		 * finished / when returning G. */
		void clear_private_data();

	public:
		/* Should only be created by depres2, but new requires the constructor
		 * to be public. */
		Depres2IGNode(
				Depres2Solver& s,
				const std::pair<const std::string, const int> &identifier,
				const bool is_selected,
				const bool installed_automatically);
	};

	/* Policy for deciding between otherwise equally suited versions of a
	 * package. Any package somehow involved in the calculation is evaluated
	 * with this policy.
	 *
	 *   * keep_newer: Try to keep the installed version. If that is not
	 *     possible, prefer the newest version.
	 *
	 *   * upgrade: Prefer the newest version.
	 *
	 *   * strong_selective_upgrade: Give the newest versions of selected
	 *     packages a strong bias.
	 */
	enum Policy
	{
		keep_newer = 0,
		upgrade = 1,
		strong_selective_upgrade = 2
	};

	/* The depres algorithm version 2.0 */
	class Depres2Solver : public SolverInterface
	{
		friend Depres2IGNode;

	protected:
		/* [(version, automatically installed)] */
		std::vector<std::pair<std::shared_ptr<PackageVersion>,bool>> installed_packages;
		std::vector<selected_package_t> selected_packages;

		cb_list_package_versions_t cb_list_package_versions;
		cb_get_package_version_t cb_get_package_version;

		installation_graph_t G;
		std::vector<std::string> errors;

		/* Active queue */
		std::list<Depres2IGNode*> active_queue;

		/* Trying to insert a node again moves it to the back of the queue */
		void insert_into_active(IGNode*);
		void remove_from_active(IGNode*);

		FileTrie<IGNode*> files;
		unsigned t_now = 0;

		/* Bias for package versions to choose */
		int policy = Policy::keep_newer;

		/* If set to true, all installed packages will be ejected at the
		 * beginning of the algorithm. */
		bool evaluate_all = false;

		/* A map of tuples (name, arch, version, alpha) which records how often
		 * the package/version combination has been chosen with the specific
		 * alpha during the algorithm's execution. This is used to detect loops
		 * in the algorithm's execution. */
		std::map<std::tuple<const std::string, const int,
			const VersionNumber, const float>, int> previous_versions;

		/* Methods for manipulating the installation graph G. */
		std::shared_ptr<IGNode> get_or_add_node(const std::pair<const std::string, const int> &identifier) override;

		/* Solver internals */
		float compute_alpha(
				const std::pair<const std::string, const int> &id,
				const IGNode* pv,
				std::shared_ptr<PackageVersion> version,
				int version_index, int versions_count);

		/* Eject a node and optionally put it into the active queue. */
		void eject_node(IGNode& v, bool put_into_active);

		/* Remove unreachable nodes - a garbage collection for nodes */
		bool is_node_unreachable(IGNode& v);
		void remove_unreachable_nodes();

		/* Formating error messages */
		void format_loop_error_message();

		/* Debug log */
		bool debug_log_enabled = false;

	public:
		virtual ~Depres2Solver() {}

		void set_parameters(
				const std::vector<std::pair<std::shared_ptr<PackageVersion>, bool>> &installed_packages,
				const std::vector<selected_package_t> &selected_packages,
				cb_list_package_versions_t cb_list_package_versions,
				cb_get_package_version_t cb_get_package_version) override;

		/* Depres2 specific configuration */
		void set_policy(int p);
		void set_evaluate_all(bool enabled);

		bool solve() override;
		std::vector<std::string> get_errors() const override;

		/* This will move the installation graph. */
		installation_graph_t get_G() override;


		/* Enabled debug log */
		void enable_debug_log(bool) override;
	};
}

#endif /* __DEPRES2_H */
