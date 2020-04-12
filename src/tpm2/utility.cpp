#include <cerrno>
#include <cstdio>
#include <cstring>
#include <system_error>
#include "utility.h"
#include "tpm2_config.h"
#include "package_meta_data.h"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
}

using namespace std;
namespace fs = std::filesystem;


void print_target(shared_ptr<Parameters> params, bool to_stderr)
{
	FILE* stream = to_stderr ? stderr : stdout;

	if (params->target_is_native())
		fprintf (stream, "Runtime system is native\n");
	else
		fprintf (stream, "Runtime system is at \"%s\"\n", params->target.c_str());
}


void run_script (std::shared_ptr<Parameters> params, ManagedBuffer<char>& script,
		const char *arg1, const char *arg2)
{
	fs::path tmp_script = create_tmp_dir(params) / "script";

	FILE *f = fopen (tmp_script.c_str(), "wb");
	if (!f)
	{
		fprintf (stderr, "Failed to write script file to %s.\n", tmp_script.c_str());
		throw system_error (error_code (errno, generic_category()));
	}

	size_t cnt_written = fwrite (script.buf, 1, script.size, f);
	fclose (f);

	if (cnt_written != script.size)
		throw gp_exception ("Failed to write to script file at " + tmp_script.string());

	if (chmod (tmp_script.c_str(), 0755) < 0)
	{
		fprintf (stderr, "Failed to make script executable.\n");
		throw system_error (error_code (errno, generic_category()));
	}


	/* Run the script */
	pid_t pid = fork();

	if (pid < 0)
	{
		fprintf (stderr, "Failed to fork to run script.\n");
		throw system_error (error_code (errno, generic_category()));
	}

	if (pid == 0)
	{
		/* In the child */
		if (!arg1)
			execl (tmp_script.c_str(), tmp_script.filename().c_str(), nullptr);
		else if (!arg2)
			execl (tmp_script.c_str(), tmp_script.filename().c_str(), arg1, nullptr);
		else
			execl (tmp_script.c_str(), tmp_script.filename().c_str(), arg1, arg2, nullptr);

		fprintf (stderr, "Failed to run script: %s\n", strerror (errno));
		exit (1);
	}

	int wstatus;

	if (waitpid (pid, &wstatus, 0) < 0)
	{
		fprintf (stderr, "Failed to wait for script to exit.\n");
		throw system_error (error_code (errno, generic_category()));
	}

	int exit_code = WEXITSTATUS (wstatus);
	if (exit_code != 0)
		throw gp_exception ("The script terminated abnormally with exit code " +
				to_string(exit_code));

	fs::remove (tmp_script);
}


fs::path create_tmp_dir (std::shared_ptr<Parameters> params)
{
	fs::path dir = simplify_path (params->target + "/" + TPM2_TMP_DIR);

	if (!fs::is_directory(dir))
		fs::create_directory (dir);

	return dir;
}


string installation_reason_to_string (char reason)
{
	switch (reason)
	{
		case INSTALLATION_REASON_AUTO:
			return "auto";

		case INSTALLATION_REASON_MANUAL:
			return "manual";

		default:
			return "invalid";
	}
}
