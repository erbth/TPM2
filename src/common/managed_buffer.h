/** This file is part of the TSClient LEGACY Package Manager
 *
 * A simple managed buffer */

#ifndef __MANAGED_BUFFER_H
#define __MANAGED_BUFFER_H

#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

template<class T>
class ManagedBuffer
{
public:
	T* buf;
	const size_t size;

	ManagedBuffer (size_t size)
		: size(size)
	{
		buf = new T[size];
	}

	/* No copy constructor */
	ManagedBuffer (const ManagedBuffer&) = delete;

	/* Move constructor */
	ManagedBuffer (ManagedBuffer&& o)
	{
		buf = o.buf;
		o.buf = nullptr;
	}

	~ManagedBuffer ()
	{
		/* The buffer may have been moved. */
		if (buf)
			delete[] buf;
	}
};


template<class T>
class DynamicBuffer
{
public:
	T* buf;
	size_t size;

	DynamicBuffer (size_t initial=1024)
	{
		buf = (T*) malloc (initial * sizeof(T));
		if (!buf)
			throw std::bad_alloc();

		size = initial;
	}

	/* No copy constructor */
	DynamicBuffer (const DynamicBuffer&) = delete;

	/* Move constructor */
	DynamicBuffer (DynamicBuffer&& o)
	{
		buf = o.buf;
		size = o.size;

		o.buf = nullptr;
		o.size = 0;
	}

	~DynamicBuffer ()
	{
		/* The buffer may have been moded. */
		if (buf)
			free (buf);
	}

	void ensure_size (size_t s)
	{
		if (s > size)
		{
			size_t new_size = size;

			while (s > new_size)
			{
				if (new_size >= std::numeric_limits<size_t>::max())
					throw std::bad_alloc();

				new_size *= 2;
			}

			T* new_buf = (T*) realloc (buf, new_size);

			if (!new_buf)
				throw std::bad_alloc();

			buf = new_buf;
			size = new_size;
		}
	}
};

#endif /* __MANAGED_BUFFER_H */
