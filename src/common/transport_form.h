/* This file is part of the TSClient LEGACY Package manager
 *
 * This modules deals with transport forms. */

#ifndef __TRANSPORT_FORM_H
#define __TRANSPORT_FORM_H

#include <optional>
#include <string>
#include <vector>
#include "package_meta_data.h"

extern "C" {
#include <zlib.h>
}


namespace TransportForm
{
	class Writer
	{
	private:
		gzFile f;


	public:
		/* @raises std::system_error if it cannot open the file. */
		Writer (const std::string& filename);
		~Writer ();

		/* @returns 0 on success or -errno value in case of error */
		int write (const char *buf, int size);
	};


	const uint8_t SEC_TYPE_DESC = 0x00;
	const uint8_t SEC_TYPE_PREINST = 0x01;
	const uint8_t SEC_TYPE_CONFIGURE = 0x02;
	const uint8_t SEC_TYPE_UNCONFIGURE = 0x03;
	const uint8_t SEC_TYPE_POSTRM = 0x04;
	const uint8_t SEC_TYPE_ARCHIVE = 0x80;
	const uint8_t SEC_TYPE_SIG_OPENPGP = 0xf0;

	struct TOCSection
	{
		uint8_t type;
		uint32_t start;
		uint32_t size;

		TOCSection (uint8_t type, uint32_t start, uint32_t size);

		static const unsigned binary_size = 9;
		void to_binary(char *buf) const;
	};

	struct TableOfContents
	{
		uint8_t version;

		std::vector<TOCSection> sections;

		unsigned binary_size() const;
		void to_binary(char *buf) const;
	};


	class TransportForm
	{
	private:
		/* If unset, an appropriate TOC will be generated each time get_toc is
		 * called. It must be set when the object represents a read transport
		 * form with a particular TOC. */
		std::optional<TableOfContents> toc;

		const char *desc = nullptr;
		size_t desc_size = 0;

		const char *preinst = nullptr;
		size_t preinst_size = 0;

		const char *configure = nullptr;
		size_t configure_size = 0;

		const char *unconfigure = nullptr;
		size_t unconfigure_size = 0;

		const char *postrm = nullptr;
		size_t postrm_size = 0;

		const uint8_t *archive = nullptr;
		size_t archive_size = 0;


	public:
		void set_desc (const char *desc, size_t size);

		void set_preinst (const char *preinst, size_t size);
		void set_configure (const char *configure, size_t size);
		void set_unconfigure (const char *unconfigure, size_t size);
		void set_postrm (const char *postrm, size_t size);

		void set_archive (const uint8_t *archive, size_t size);

		TableOfContents get_toc() const;


		/* @returns 0 on success or -errno value in case of error */
		int write (Writer& w);
	};


	std::string filename_from_mdata (const PackageMetaData& mdata);
};

#endif /* __TRANSPORT_FORM_H */
