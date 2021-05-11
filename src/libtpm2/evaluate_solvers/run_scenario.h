/** This file is part of the TSClient LEGACY Package Manager.
 *
 * Classes around solving a specific scenario with a specific solver and
 * evaluating how good the solver meet the desired configuration. */
#ifndef __RUN_SCENARIO_H
#define __RUN_SCENARIO_H

#include <memory>
#include <utility>
#include <vector>
#include "depres_common.h"
#include "read_scenario.h"

/* Abstract type that can represent an installed or a provided package */
class PackageVersionAdaptor : public PackageVersion
{
protected:
	std::shared_ptr<Scenario::Package> scenario_package;

public:
	PackageVersionAdaptor(std::shared_ptr<Scenario::Package> scenario_package);

	std::vector<
			std::pair<std::pair<std::string, int>, std::shared_ptr<const PackageConstraints::Formula>>
		> get_dependencies() override;

	std::vector<
			std::pair<std::pair<std::string, int>, std::shared_ptr<const PackageConstraints::Formula>>
		> get_pre_dependencies() override;

	const std::vector<std::string> &get_files() override;
	const std::vector<std::string> &get_directories() override;
};

/* Installed package version */
class InstalledPackageVersionAdaptor : public PackageVersionAdaptor
{
public:
	InstalledPackageVersionAdaptor(std::shared_ptr<Scenario::Package> scenario_package)
		: PackageVersionAdaptor(scenario_package)
	{
	}

	bool is_installed() const override
	{
		return true;
	}
};

/* Provided package version */
class ProvidedPackageVersionAdaptor : public PackageVersionAdaptor
{
public:
	ProvidedPackageVersionAdaptor(std::shared_ptr<Scenario::Package> scenario_package)
		: PackageVersionAdaptor(scenario_package)
	{
	}

	bool is_installed() const override
	{
		return false;
	}
};

class ScenarioRunner
{
protected:
	std::shared_ptr<depres::SolverInterface> solver;
	std::shared_ptr<Scenario> scenario;

	/* Adapted versions of the scenario that was read from an xml file s.t. they
	 * meet the SolveInterface. */
	std::vector<std::shared_ptr<PackageVersion>> adapted_universe;
	std::vector<std::pair<std::shared_ptr<PackageVersion>,bool>> adapted_installed_packages;
	std::vector<std::pair<std::pair<std::string, int>, std::shared_ptr<const PackageConstraints::Formula>>>
		adapted_selected_packages;

	std::vector<std::shared_ptr<PackageVersion>> result;

	/* Callback functions that are passed to the solver. Those would usually be
	 * provided by the package provider, but in this case we emulate the package
	 * provider. */
	std::vector<VersionNumber> list_package_versions (const std::string &name, int arch);
	std::shared_ptr<PackageVersion> get_package_version (
			const std::string &name, int arch, const VersionNumber &version);

	/* Result of the last solver run */
	bool solver_succeeded = false;
	std::vector<std::string> solver_errors;
	depres::installation_graph_t solver_G;

public:
	void set_solver(std::shared_ptr<depres::SolverInterface>);
	void set_scenario(std::shared_ptr<Scenario> scenario);

	void run();

	/** Evaluate the result against the desired configuration. Give it a
	 * deviation and return it (that is 0 means the desired result has been
	 * matched perfectly).
	 *
	 * @returns (deviation, reasons) */
	std::pair<double, std::vector<std::string>> evaluate_result();
};

#endif /* __RUN_SCENARIO_H */
