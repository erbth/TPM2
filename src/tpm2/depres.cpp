#include "depres.h"
#include <set>
#include "architecture.h"

using namespace std;


namespace depres
{

ComputeInstallationGraphResult compute_installation_graph(
		std::shared_ptr<Parameters> params,
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

}
