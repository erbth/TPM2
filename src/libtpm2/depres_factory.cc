#include "depres_factory.h"
#include "depres2.h"

using namespace std;

namespace depres
{

shared_ptr<SolverInterface> create_solver(const string &solver_name)
{
	if (solver_name == "depres2") {
		return make_shared<Depres2Solver>();
	}

	return nullptr;
}

}
