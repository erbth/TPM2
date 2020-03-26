#include "depres.h"
#include <set>
#include "architecture.h"

using namespace std;


namespace depres
{

ComputeInstallationGraphResult compute_installation_graph(
		std::vector<std::shared_ptr<PackageMetaData>> installed_packages,
		std::vector<
			std::pair<
				std::pair<const std::string, int>,
				std::shared_ptr<const PackageConstraints::Formula>
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
		g.add(make_shared<InstallationGraphNode>(
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
						CIG_INVALID_CURRENT_CONFIG,
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
						CIG_INVALID_CURRENT_CONFIG,
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


	/* Check if the current configuration is valid. That is, apart from what has
	 * already been done above, the chosen versions must fulfill the constraints
	 * of dependencies and the installed packages must not conflict. */

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
}


void InstallationGraph::add(shared_ptr<InstallationGraphNode> n)
{
	V.insert({{n->name, n->architecture}, n});
}

}
