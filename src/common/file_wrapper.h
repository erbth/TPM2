/** This file is part of the TSClient LEGACY Package Manager
 *
 * A simple wrapper for C file streams. */

#ifndef __FILE_WRAPPER_H
#define __FILE_WRAPPER_H

#include <cerrno>
#include <cstdio>

class FileWrapper
{
private:
	FILE* f = nullptr;

public:
	FileWrapper ()
	{
	}

	~FileWrapper ()
	{
		close();
	}

	/* Opens a file. If another file is currently opened, it will be closed
	 * first.
	 *
	 * @param path Path to the file to open
	 * @param mode Will be passed as mode parameter to fopen
	 *
	 * @returns 0 on success or -errno value on failure. That works because "The
	 * value of errno is never set to zero by any system call or library
	 * function." (from errno(3)). */
	int open (const char* path, const char* mode)
	{
		close();

		f = fopen (path, mode);
		if (!f)
			return -errno;
		else
			return 0;
	}

	/* Close the underlying file stream if it is open. This will be called by
	 * the destructor automatically if not invoked before. */
	void close ()
	{
		if (f)
		{
			fclose (f);
			f = nullptr;
		}
	}

	bool is_open()
	{
		return f != nullptr;
	}

	bool eof ()
	{
		return f && feof (f) != 0;
	}

	bool error ()
	{
		return f && ferror (f) != 0;
	}

	size_t read (void *buf, size_t size)
	{
		return f ? fread (buf, 1, size, f) : 0;
	}

	size_t write (void *buf, size_t size)
	{
		return f ? fwrite (buf, 1, size, f) : 0;
	}

	/* @param off     The offset to seek to
	 * @param whence  The reference for offset; will be directly passed to fseek
	 *                Hence use SEEK_SET, SEEK_CUR and SEEK_END.
	 * @returns       0 on success or -errno value on failure. */
	int seek (long off, int whence)
	{
		if (f)
		{
			int r = fseek (f, off, whence);

			if (r == 0)
				return 0;
			else
				return -errno;
		}
		else
		{
			return -ENOENT;
		}
	}

	/* @returns The current position on success or -errno value in case of
	 * failure */
	long tell ()
	{
		if (f)
		{
			long pos = ftell (f);

			if (pos < 0)
				return -errno;
			else
				return pos;
		}
		else
		{
			return -ENOENT;
		}
	}
};

#endif /* __FILE_WRAPPER_H */
