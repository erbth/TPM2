/** This file is part of the TSClient LEGACY Package Manager
 *
 * An executble program to run a solver on a scenario. */
#include <cstdio>
#include "run_scenario.h"
#include "depres_factory.h"

using namespace std;


int main (int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s <scenario.xml>\n", argv[0]);
		return 1;
	}

	/* Create the solver */
	auto solver = depres::create_solver("depres2");
	if (!solver)
	{
		fprintf (stderr, "Failed to create solver.\n");
		return 1;
	}

	/* Read the scenario */
	auto scenario = read_scenario(string(argv[1]));
	if (!scenario)
	{
		fprintf (stderr, "Failed to read scenario.\n");
		return 1;
	}

	/* Run the solver */
	ScenarioRunner runner;
	runner.set_solver(solver);
	runner.set_scenario(scenario);
	runner.run();

	/* Display the result. */
	auto [deviation, reasons] = runner.evaluate_result();

	printf ("Deviation: %f\n", deviation);
	printf ("Reasons:\n");

	if (reasons.size())
	{
		for (auto &reason : reasons)
			printf ("  %s\n", reason.c_str());
	}
	else
	{
		printf ("  ---\n");
	}

	return 0;
}
