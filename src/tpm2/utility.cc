#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <regex>
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
namespace pc = PackageConstraints;


void print_target(shared_ptr<Parameters> params, bool to_stderr)
{
	FILE* stream = to_stderr ? stderr : stdout;

	if (params->target_is_native())
	{
		if (params->verbose)
			fprintf (stream, "Runtime system is native\n");
	}
	else
	{
		fprintf (stream, "Runtime system is at \"%s\"\n", params->target.c_str());
	}
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
		/* Set environment_variables */
		if (setenv ("TPM_TARGET", params->target.c_str(), 1) < 0)
		{
			fprintf (stderr, "Failed to set environment variable: %s\n", strerror (errno));
			exit (1);
		}

		/* Execute the script */
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



parse_cmd_param_result parse_cmd_param (const Parameters& params, const std::string& pkg)
{
	parse_cmd_param_result res (pkg);
	res.success = true;
	res.arch = params.default_architecture;

	/* May be of the form name@arch>=version */
	const regex pattern1(
			"([^<>!=@]+)[ \t]*(@(amd64|i386))?[ \t]*((>=|<=|>|<|=|==|!=)(s:)?([^<>!=@]+))?");

	smatch m1;
	if (regex_match (pkg, m1, pattern1))
	{
		res.name = m1[1].str();
		
		if (m1[4].matched)
		{
			auto op = m1[5];

			char type;

			if (op == ">=")
			{
				type = pc::PrimitivePredicate::TYPE_GEQ;
			}
			else if (op == "<=")
			{
				type = pc::PrimitivePredicate::TYPE_LEQ;
			}
			else if (op == ">")
			{
				type = pc::PrimitivePredicate::TYPE_GT;
			}
			else if (op == "<")
			{
				type = pc::PrimitivePredicate::TYPE_LT;
			}
			else if (op == "=" || op == "==")
			{
				type = pc::PrimitivePredicate::TYPE_EQ;
			}
			else
			{
				type = pc::PrimitivePredicate::TYPE_NEQ;
			}

			try
			{
				res.vc = make_shared<pc::PrimitivePredicate>(
						m1[6].matched, type, VersionNumber(m1[7].str()));
			}
			catch (InvalidVersionNumberString& e)
			{
				res.err = e.what();
				res.success = false;
				return res;
			}
		}

		if (m1[2].matched)
		{
			res.arch = Architecture::from_string (m1[3].str());
		}

		return res;
	}

	res.success = false;
	res.err = "Unknown format";
	return res;
}


string pkg_state_to_string (int state)
{
	switch (state)
	{
		case PKG_STATE_INVALID:
			return "invalid";

		case PKG_STATE_WANTED:
			return "wanted";

		case PKG_STATE_PREINST_BEGIN:
			return "preinst_begin";

		case PKG_STATE_UNPACK_BEGIN:
			return "unpack_begin";

		case PKG_STATE_CONFIGURE_BEGIN:
			return "configure_begin";

		case PKG_STATE_CONFIGURED:
			return "configured";

		case PKG_STATE_UNCONFIGURE_BEGIN:
			return "unconfigure_begin";

		case PKG_STATE_RM_FILES_BEGIN:
			return "rm_files_begin";

		case PKG_STATE_POSTRM_BEGIN:
			return "postrm_begin";

		case PKG_STATE_UNCONFIGURE_CHANGE:
			return "unconfigure_change";

		case PKG_STATE_WAIT_NEW_UNPACKED:
			return "wait_new_unpacked";

		case PKG_STATE_RM_FILES_CHANGE:
			return "rm_files_change";

		case PKG_STATE_POSTRM_CHANGE:
			return "postrm_change";

		case PKG_STATE_PREINST_CHANGE:
			return "preinst_change";

		case PKG_STATE_UNPACK_CHANGE:
			return "unpack_change";

		case PKG_STATE_WAIT_OLD_REMOVED:
			return "wait_old_removed";

		case PKG_STATE_CONFIGURE_CHANGE:
			return "configure_change";

		default:
			return "???";
	};
}


void printf_verbose (shared_ptr<Parameters> params, const char* fmt, ...)
{
	if (params->verbose)
	{
		va_list ap;
		va_start(ap, fmt);
		vprintf (fmt, ap);
		va_end(ap);
	}
}

void printf_verbose_flush (shared_ptr<Parameters> params, const char* fmt, ...)
{
	if (params->verbose)
	{
		va_list ap;
		va_start(ap, fmt);
		vprintf (fmt, ap);
		va_end(ap);

		fflush(stdout);
	}
}
