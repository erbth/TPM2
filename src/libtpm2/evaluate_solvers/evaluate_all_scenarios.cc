/** This file is part of the TSClient LEGACY Package Manager */
#include <cmath>
#include <cstdio>
#include "evaluate_all_scenarios.h"
#include "depres_factory.h"

using namespace std;
namespace fs = std::filesystem;


AllScenarioEvaluator::AllScenarioEvaluator(const string& solver_name)
	: solver_name(solver_name)
{
}

void AllScenarioEvaluator::set_scenario_path(const fs::path &path)
{
	scenario_path = path;
}

void AllScenarioEvaluator::find_scenarios()
{
	for (auto &file : fs::directory_iterator(scenario_path))
	{
		if (file.path().extension() == "xml" ||
				(((string) file.path().filename()).size() >= 1 && ((string) file.path().filename())[0] == '.'))
			continue;

		if (file.is_regular_file())
			scenarios.push_back(make_pair(file.path().stem(), file.path()));
	}
}

vector<string> AllScenarioEvaluator::list_scenarios() const
{
	vector<string> names;

	for (auto &[n,p] : scenarios)
		names.push_back(n);

	return names;
}

void AllScenarioEvaluator::solve_all()
{
	overall_deviation = 0;
	bool failed = false;

	for (auto &[name,path] : scenarios)
	{
		ScenarioRunner runner;
		runner.set_solver(depres::create_solver(solver_name));

		printf ("Evaluating scenario %s ...\n", name.c_str());
		auto scenario = read_scenario(path);
		if (!scenario)
		{
			fprintf (stderr, "Failed to read the scenario.\n");
			overall_deviation = -INFINITY;
			return;
		}

		runner.set_scenario(scenario);
		runner.run();

		auto [deviation,reasons] = runner.evaluate_result();

		overall_deviation += fabs(deviation);
		if (deviation < 0)
			failed = true;

		printf ("  Deviation: %f\n    Reasons:\n", deviation);
		if (reasons.size())
		{
			for (auto &reason : reasons)
				printf ("      %s\n", reason.c_str());
		}
		else 
		{
			printf ("      --\n");
		}

		printf ("\n");
	}

	if (failed)
		overall_deviation *= -1;
}

double AllScenarioEvaluator::get_overall_deviation()
{
	return overall_deviation;
}
