/* This file is part of the TSClient LEGACY Package manager
 *
 * This modules deals with transport forms. */

#ifndef __TRANSPORT_FORM_H
#define __TRANSPORT_FORM_H

#include <exception>
#include <optional>
#include <string>
#include <vector>
#include "package_meta_data.h"
#include "file_list.h"

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
		Writer (const Writer& o) = delete;

		~Writer ();

		/* @returns 0 on success or -errno value in case of error */
		int write (const char *buf, int size);
	};


	class ReadStream
	{
	protected:
		ReadStream();

	public:
		virtual ~ReadStream() = 0;

		virtual std::string get_filename() const;

		virtual void read (char *buf, size_t cnt) = 0;
		virtual size_t tell () = 0;
		virtual void seek (size_t pos) = 0;
	};

	class GZReadStream : public ReadStream
	{
	protected:
		gzFile f;
		const std::string filename;

	public:
		/* @raises std::system_error if it cannot open the file. */
		GZReadStream (const std::string& filename);
		GZReadStream (const GZReadStream& o) = delete;

		virtual ~GZReadStream();

		std::string get_filename() const override;

		/* If not all data could be read, the function raises an exception.
		 * @raises std::system_error */
		void read (char *buf, size_t cnt) override;

		/* @raises std::system_error */
		size_t tell () override;

		/* Absolute seek. "[C]an be extremely slow" (zlib manual) ...
		 * @raises std::system_error */
		void seek (size_t pos) override;
	};

	class FDReadStream : public ReadStream
	{
	protected:
		int fd;
		bool close;

	public:
		/** @param close  If true, the file descriptor will be closed when the
		 * 		object is destroyed. */
		FDReadStream(int fd, bool close);
		virtual ~FDReadStream();

		void read (char *buf, size_t cnt) override;
		size_t tell () override;
		void seek (size_t pos) override;
	};


	const uint8_t SEC_TYPE_DESC = 0x00;
	const uint8_t SEC_TYPE_FILE_INDEX = 0x01;
	const uint8_t SEC_TYPE_CONFIG_FILES = 0x02;
	const uint8_t SEC_TYPE_PREINST = 0x20;
	const uint8_t SEC_TYPE_CONFIGURE = 0x21;
	const uint8_t SEC_TYPE_UNCONFIGURE = 0x22;
	const uint8_t SEC_TYPE_POSTRM = 0x23;
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

		/* @raises InvalidToc and what rs raises */
		static TOCSection read_from_binary (ReadStream& rs);
	};

	struct TableOfContents
	{
		uint8_t version;

		std::vector<TOCSection> sections;

		unsigned binary_size() const;
		void to_binary(char *buf) const;

		/* @raises InvalidToc and what rs raises. */
		static TableOfContents read_from_binary (ReadStream& rs);
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

		const char *file_index = nullptr;
		size_t file_index_size = 0;

		const char *config_files = nullptr;
		size_t config_files_size = 0;

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
		void set_file_index (const char *file_index, size_t size);
		void set_config_files (const char *config_files, size_t size);

		void set_preinst (const char *preinst, size_t size);
		void set_configure (const char *configure, size_t size);
		void set_unconfigure (const char *unconfigure, size_t size);
		void set_postrm (const char *postrm, size_t size);

		void set_archive (const uint8_t *archive, size_t size);

		TableOfContents get_toc() const;


		/* @returns 0 on success or -errno value in case of error */
		int write (Writer& w);
	};


	/* A lightweight version of the transport form that serves as an interface
	 * between the transport form module, which reads the packed file, and the
	 * package provider. */
	struct ReadTransportForm
	{
		TableOfContents toc;
		std::shared_ptr<PackageMetaData> mdata;
	};

	/* @raises what rs raises and InvalidToc,
	 *     invalid_package_meta_data_xml. */
	ReadTransportForm read_transport_form (ReadStream& rs);


	std::string filename_from_mdata (const PackageMetaData& mdata);


	/* This will always return a valid pointer, maybe to an empty list. */
	std::shared_ptr<FileList> read_file_list (ReadStream& rs, size_t size);

	/* This will always return a valid pointer, maybe to an empty list. The list
	 * is sorted in ascending order. */
	std::shared_ptr<std::vector<std::string>> read_config_files (ReadStream& rs, size_t size);


	class InvalidToc : public std::exception
	{
	private:
		std::string msg;

	public:
		InvalidToc (const std::string& file, const std::string& msg);
		const char *what() const noexcept override;
	};
};

#endif /* __TRANSPORT_FORM_H */
