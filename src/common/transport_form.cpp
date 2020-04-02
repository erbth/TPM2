#include <cerrno>
#include <cstring>
#include <system_error>
#include "transport_form.h"
#include "architecture.h"
#include "managed_buffer.h"

extern "C" {
#include <endian.h>
}

using namespace std;


namespace TransportForm
{


/* A file writer */
Writer::Writer (const string& filename)
{
	f = gzopen (filename.c_str(), "we");

	if (!f)
		throw system_error (error_code (errno, generic_category()));
}


Writer::~Writer ()
{
	gzclose (f);
}


int Writer::write (const char *buf, int size)
{
	int cnt = gzwrite (f, buf, size);

	if (cnt != size)
		return -errno;
	else
		return 0;
}


TOCSection::TOCSection (uint8_t type, uint32_t start, uint32_t size)
	: type(type), start(start), size(size)
{
}


void TOCSection::to_binary (char *buf) const
{
	buf[0] = (uint8_t) type;
	*((uint32_t*) (buf + 1)) = htole32 (start);
	*((uint32_t*) (buf + 5)) = htole32 (size);
}


unsigned TableOfContents::binary_size() const
{
	return 2 + sections.size() * TOCSection::binary_size;
}


void TableOfContents::to_binary (char *buf) const
{
	buf[0] = (uint8_t) version;
	buf[1] = (uint8_t) sections.size();

	for (size_t i = 0; i < sections.size(); i++)
		sections[i].to_binary (buf + 2 + i * TOCSection::binary_size);
}


size_t FileRecord::binary_size() const
{
	return 1 + 4 + 4 + 2 + 4 + 20 + path.size() + 1;
}


void FileRecord::to_binary (uint8_t *buf) const
{
	*buf = type;
	*((uint32_t*) (buf + 0x01)) = htole32 (uid);
	*((uint32_t*) (buf + 0x05)) = htole32 (gid);
	*((uint16_t*) (buf + 0x09)) = htole16 (mode);
	*((uint32_t*) (buf + 0x0b)) = htole32 (size);
	strncpy ((char*) buf + 0x0f, (char*) sha1_sum, 20);
	strcpy ((char*) buf + 0x23, path.c_str());
	*(buf + 0x23 + path.size()) = 0;
}


/* A class that represents a transport form */
void TransportForm::set_desc (const char *desc, size_t size)
{
	this->desc = desc;
	this->desc_size = size;
}

void TransportForm::set_file_index (const char *file_index, size_t size)
{
	this->file_index = file_index;
	file_index_size = size;
}

void TransportForm::set_preinst (const char *preinst, size_t size)
{
	this->preinst = preinst;
	this->preinst_size = size;
}

void TransportForm::set_configure (const char *configure, size_t size)
{
	this->configure = configure;
	this->configure_size = size;
}

void TransportForm::set_unconfigure (const char *unconfigure, size_t size)
{
	this->unconfigure = unconfigure;
	this->unconfigure_size = size;
}

void TransportForm::set_postrm (const char *postrm, size_t size)
{
	this->postrm = postrm;
	this->postrm_size = size;
}

void TransportForm::set_archive (const uint8_t *archive, size_t size)
{
	this->archive = archive;
	this->archive_size = size;
}


TableOfContents TransportForm::get_toc() const
{
	/* If there is a stored toc, return it. */
	if (toc)
		return toc.value();

	/* Otherwise construct an appropriate one. */
	TableOfContents t;

	/* First build the toc and update the positions later once the toc's size is
	 * clear. */
	t.version = 1;

	t.sections.push_back (TOCSection (SEC_TYPE_DESC, 0, desc_size));


	/* Add file index */
	if (file_index)
		t.sections.push_back (TOCSection (SEC_TYPE_FILE_INDEX, 0, file_index_size));


	/* Add maintainer scripts if present */
	if (preinst)
		t.sections.push_back (TOCSection (SEC_TYPE_PREINST, 0, preinst_size));

	if (configure)
		t.sections.push_back (TOCSection (SEC_TYPE_CONFIGURE, 0, configure_size));

	if (unconfigure)
		t.sections.push_back (TOCSection (SEC_TYPE_UNCONFIGURE, 0, unconfigure_size));

	if (postrm)
		t.sections.push_back (TOCSection (SEC_TYPE_POSTRM, 0, postrm_size));


	/* Add archive */
	if (archive)
		t.sections.push_back (TOCSection (SEC_TYPE_ARCHIVE, 0, archive_size));


	/* Update offsets */
	uint32_t pos = 2;

	for (auto& s : t.sections)
	{
		s.start = pos;
		pos += s.size;
	}

	return t;
}


int TransportForm::write (Writer& w)
{
	if (!desc)
		return -ENOMEM;

	if ((file_index == nullptr) != (archive == nullptr))
		return -ENOMEM;

	/* Write toc */
	{
		TableOfContents toc = get_toc();
		auto toc_size = toc.binary_size();

		ManagedBuffer<char> buf(toc_size);
		toc.to_binary (buf.buf);

		auto r = w.write (buf.buf, toc_size);
		if (r != 0)
			return r;
	}

	/* Write desc */
	auto r = w.write (desc, desc_size);
	if (r != 0)
		return r;

	/* Write file index if there */
	if (file_index)
	{
		r = w.write (file_index, file_index_size);
		if (r != 0)
			return r;
	}

	/* Write maintainer scripts if present */
	if (preinst)
	{
		r = w.write (preinst, preinst_size);
		if (r != 0)
			return r;
	}

	if (configure)
	{
		r = w.write (configure, configure_size);
		if (r != 0)
			return r;
	}

	if (unconfigure)
	{
		r = w.write (unconfigure, unconfigure_size);
		if (r != 0)
			return r;
	}

	if (postrm)
	{
		r = w.write (postrm, postrm_size);
		if (r != 0)
			return r;
	}

	/* Write archive if there */
	if (archive)
	{
		r = w.write ((const char*) archive, archive_size);
		if (r != 0)
			return r;
	}

	return 0;
}


string filename_from_mdata (const PackageMetaData& mdata)
{
	return mdata.name + "-" + mdata.version.to_string() + "_" +
		Architecture::to_string (mdata.architecture) + ".tpm2";
}


}
