#include <cstdio>
#include "evaluate_all_scenarios.cc"

int main(int argc, char** argv)
{
	if (argc != 1)
	{
		fprintf (stderr, "Usage: %s\n", argv[0]);
		return 1;
	}

	AllScenarioEvaluator e("depres2");

	e.find_scenarios();

	printf ("Found the following scenarios:\n");
	for (auto &sc : e.list_scenarios())
		printf ("    %s\n", sc.c_str());

	printf ("\n");

	/* Solve all scenarios */
	e.solve_all();

	/* Print the overall result */
	double overall_deviation = e.get_overall_deviation();
	printf ("Overall deviation: %f\n", overall_deviation);

	return overall_deviation == 0 ? 0 : 1;
}
