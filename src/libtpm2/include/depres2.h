/** This file is part of the TSClient LEGACY Package Manager
 *
 * Depres version 2
 *
 * TODO:
 *   * user_contradiction_1
 *   * user_contradiction_2
 *   * cycle_oscillation
 *
 *   * conflict through files (actual conflicts between packages)
 *   * removing packages (? - would enable to move files between packages
 *     without having to provide empty versions of the packages which previously
 *     owned the files)
 * */
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
#include "depres_common.h"


namespace depres
{
	/* The depres algorithm version 2.0 */
	class Depres2Solver : public SolverInterface
	{
	protected:
		/* [(version, automatically installed)] */
		std::vector<std::pair<std::shared_ptr<PackageVersion>,bool>> installed_packages;
		std::vector<selected_package_t> selected_packages;

		cb_list_package_versions_t cb_list_package_versions;
		cb_get_package_version_t cb_get_package_version;

		installation_graph_t G;
		std::set<IGNode*> active;

		std::vector<std::string> errors;

		/* Methods for manipulating the installation graph G. */
		IGNode &get_or_add_node(const std::pair<const std::string, const int> &identifier) override;

		/* Solver internals */
		float compute_alpha(
				const std::pair<const std::string, const int> &id,
				const IGNode* pv,
				std::shared_ptr<PackageVersion> version,
				int version_index, int versions_count);

	public:
		virtual ~Depres2Solver() {}

		void set_parameters(
				const std::vector<std::pair<std::shared_ptr<PackageVersion>, bool>> &installed_packages,
				const std::vector<selected_package_t> &selected_packages,
				cb_list_package_versions_t cb_list_package_versions,
				cb_get_package_version_t cb_get_package_version) override;

		bool solve() override;
		std::vector<std::string> get_errors() const override;

		/* This will move the installation graph. */
		installation_graph_t get_G() override;
	};
}

#endif /* __DEPRES2_H */
