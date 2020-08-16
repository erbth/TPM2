/** This file is part of the TSClient LEGACY Package Manager
 *
 * Run and evaluate all scenarios, giving the solver an overall deviation. */
#ifndef __EVALUATE_ALL_SCENARIOS_H
#define __EVALUATE_ALL_SCENARIOS_H

#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include "run_scenario.h"

#include "evaluate_solvers_config.h"

class AllScenarioEvaluator
{
protected:
	std::filesystem::path scenario_path = DEFAULT_SCENARIO_PATH;

	std::shared_ptr<depres::SolverInterface> solver;

	/* All scenarios in a vector of pairs <name, path> */
	std::vector<std::pair<std::string, std::filesystem::path>> scenarios;

	double overall_deviation {};

public:
	/* Optional; if not called, a default will be used */
	void set_scenario_path(const std::filesystem::path &path);

	void set_solver(std::shared_ptr<depres::SolverInterface> solver);

	void find_scenarios();
	std::vector<std::string> list_scenarios() const;

	void solve_all();
	double get_overall_deviation();
};

#endif /* __EVALUATE_ALL_SCENARIOS_H */
