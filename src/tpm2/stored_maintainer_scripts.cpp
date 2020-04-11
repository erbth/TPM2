#include <cerrno>
#include <system_error>
#include "stored_maintainer_scripts.h"
#include "utility.h"

using namespace std;
namespace fs = std::filesystem;
namespace tf = TransportForm;


StoredMaintainerScripts::StoredMaintainerScripts (
		shared_ptr<Parameters> params,
		shared_ptr<PackageMetaData> mdata)
	: params(params), mdata(mdata)
{
	/* Read the toc */
	if (fs::is_regular_file (get_path ()))
	{
		ensure_read_stream();
		toc = tf::TableOfContents::read_from_binary (*rs);
	}
	else
	{
		toc.version = 1;
	}
}


StoredMaintainerScripts::StoredMaintainerScripts (
		shared_ptr<Parameters> params,
		shared_ptr<PackageMetaData> mdata,
		shared_ptr<ManagedBuffer<char>> preinst,
		shared_ptr<ManagedBuffer<char>> configure,
		shared_ptr<ManagedBuffer<char>> unconfigure,
		shared_ptr<ManagedBuffer<char>> postrm)
	:
		params(params), mdata(mdata),
		preinst(preinst), configure(configure),
		unconfigure(unconfigure), postrm(postrm)
{
	/* Build toc */
	toc.version = 1;

	if (preinst)
		toc.sections.push_back (tf::TOCSection (tf::SEC_TYPE_PREINST, 0, preinst->size));

	if (configure)
		toc.sections.push_back (tf::TOCSection (tf::SEC_TYPE_CONFIGURE, 0, configure->size));

	if (unconfigure)
		toc.sections.push_back (tf::TOCSection (tf::SEC_TYPE_UNCONFIGURE, 0, unconfigure->size));

	if (postrm)
		toc.sections.push_back (tf::TOCSection (tf::SEC_TYPE_POSTRM, 0, postrm->size));

	/* Calculate section offsets */
	uint32_t pos = toc.binary_size();

	for (auto& s : toc.sections)
	{
		s.start = pos;
		pos += s.size;
	}
}


fs::path StoredMaintainerScripts::get_path () const
{
	return simplify_path (params->target + "/var/lib/tpm/" + mdata->name + "-" +
			mdata->version.to_string() + "_" +
			Architecture::to_string (mdata->architecture) + ".tpm2sms");
}


void StoredMaintainerScripts::ensure_read_stream()
{
	if (!rs)
		rs = make_unique<tf::ReadStream> (get_path());
}


shared_ptr<ManagedBuffer<char>> StoredMaintainerScripts::get_preinst()
{
	if (!preinst)
	{
		for (const auto& sec : toc.sections)
		{
			if (sec.type == tf::SEC_TYPE_PREINST)
			{
				if (sec.size > 0)
				{
					ensure_read_stream();

					if (rs->tell() != sec.start)
						rs->seek (sec.start);

					preinst = make_shared<ManagedBuffer<char>> (sec.size);
					rs->read (preinst->buf, sec.size);
				}

				break;
			}
		}
	}

	return preinst;
}


shared_ptr<ManagedBuffer<char>> StoredMaintainerScripts::get_configure()
{
	if (!configure)
	{
		for (const auto& sec : toc.sections)
		{
			if (sec.type == tf::SEC_TYPE_CONFIGURE)
			{
				if (sec.size > 0)
				{
					ensure_read_stream();

					if (rs->tell() != sec.start)
						rs->seek (sec.start);

					configure = make_shared<ManagedBuffer<char>> (sec.size);
					rs->read (configure->buf, sec.size);
				}

				break;
			}
		}
	}

	return configure;
}


shared_ptr<ManagedBuffer<char>> StoredMaintainerScripts::get_unconfigure()
{
	if (!unconfigure)
	{
		for (const auto& sec : toc.sections)
		{
			if (sec.type == tf::SEC_TYPE_UNCONFIGURE)
			{
				if (sec.size > 0)
				{
					ensure_read_stream();

					if (rs->tell() != sec.start)
						rs->seek (sec.start);

					unconfigure = make_shared<ManagedBuffer<char>> (sec.size);
					rs->read (unconfigure->buf, sec.size);
				}

				break;
			}
		}
	}

	return unconfigure;
}


shared_ptr<ManagedBuffer<char>> StoredMaintainerScripts::get_postrm()
{
	if (!postrm)
	{
		for (const auto& sec : toc.sections)
		{
			if (sec.type == tf::SEC_TYPE_POSTRM)
			{
				if (sec.size > 0)
				{
					ensure_read_stream();

					if (rs->tell() != sec.start)
						rs->seek (sec.start);

					postrm = make_shared<ManagedBuffer<char>> (sec.size);
					rs->read (postrm->buf, sec.size);
				}

				break;
			}
		}
	}

	return postrm;
}


void StoredMaintainerScripts::clear_buffers()
{
	if (rs)
	{
		preinst = configure = unconfigure = postrm = nullptr;
		rs.reset();
	}
}


void StoredMaintainerScripts::write () const
{
	if (preinst || configure || unconfigure || postrm)
	{
		tf::Writer w(get_path());

		/* Write toc */
		{
			ManagedBuffer<char> buf(toc.binary_size());
			toc.to_binary (buf.buf);

			auto r = w.write (buf.buf, buf.size);
			if (r < 0)
			{
				fprintf (stderr, "Failed to write stored maintainer scripts\n");
				throw system_error (error_code (-r, generic_category()));
			}
		}

		if (preinst)
		{
			auto r = w.write (preinst->buf, preinst->size);
			if (r < 0)
			{
				fprintf (stderr, "Failed to write preinst to sms\n");
				throw system_error (error_code (-r, generic_category()));
			}
		}

		if (configure)
		{
			auto r = w.write (configure->buf, configure->size);
			if (r < 0)
			{
				fprintf (stderr, "Failed to write configure to sms\n");
				throw system_error (error_code (-r, generic_category()));
			}
		}

		if (unconfigure)
		{
			auto r = w.write (unconfigure->buf, unconfigure->size);
			if (r < 0)
			{
				fprintf (stderr, "Failed to write unconfigure to sms\n");
				throw system_error (error_code (-r, generic_category()));
			}
		}

		if (postrm)
		{
			auto r = w.write (postrm->buf, postrm->size);
			if (r < 0)
			{
				fprintf (stderr, "Failed to write postrm to sms\n");
				throw system_error (error_code (-r, generic_category()));
			}
		}
	}
}
