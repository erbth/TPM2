/** This file if part of the TSClient LEGACY Package Manager
 *
 * This module contains a trie as file storage */

#ifndef __FILE_TRIE_H
#define __FILE_TRIE_H

#include <map>
#include <string>
#include "common_utilities.h"


/* Prototypes */
template<typename T> class FileTrie;
template<typename T> class FileTrieNode;
template<typename T> class FileTrieTestAdaptor;


template<typename T>
class FileTrieNodeHandle
{
	friend FileTrie<T>;
	friend FileTrieTestAdaptor<T>;

private:
	FileTrieNode<T> *ptr;

	FileTrieNodeHandle () : ptr(nullptr) {}
	FileTrieNodeHandle (FileTrieNode<T> *ptr) : ptr(ptr) {}

public:
	FileTrieNodeHandle (const FileTrieNodeHandle& o) : ptr(o.ptr) {}
	FileTrieNodeHandle (FileTrieNodeHandle&& o) : ptr(o.ptr) { o.ptr = nullptr; }

	FileTrieNode<T>& operator*() const noexcept { return *ptr; }
	FileTrieNode<T>* operator->() const noexcept { return ptr; }

	explicit operator bool() const noexcept { return ptr != nullptr; }

	bool operator== (FileTrieNodeHandle<T> o) const noexcept { return ptr == o.ptr; };
	bool operator!= (FileTrieNodeHandle<T> o) const noexcept { return ptr != o.ptr; };
};


template<typename T>
class FileTrie
{
	friend FileTrieTestAdaptor<T>;

private:
	/* Children of the root directory */
	std::map<std::string, FileTrieNode<T>> children;

	/* Trailing slash marks directory */
	void insert_element (const std::string& _path)
	{
		/* Elliminate double slashes while keeping a potential trailing slash */
		std::string path = simplify_path ('/' + _path);

		std::string::size_type begin = 0;
		std::string::size_type end = path.size();

		FileTrieNode<T> *current_node = nullptr;

		for (;;)
		{
			std::string::size_type pos;
			std::string sub;

			if (begin < end)
			{
				pos = path.find ('/', begin);

				if (pos == 0)
				{
					begin = 1;
					continue;
				}

				if (pos == std::string::npos)
					pos = end;

				sub = path.substr (begin, pos - begin);
			}
			else
			{
				pos = end;
				sub = "";
			}


			auto i = current_node ? current_node->children.find (sub) : children.find (sub);

			if (i != (current_node ? current_node->children.end() : children.end()))
			{
				current_node = &i->second;

				/* Only the last component is a leaf, so if we encounter a
				 * component that is a leaf, it's safe to abort. */
				if (current_node->is_leaf)
					break;
			}
			else
			{
				auto& c = current_node ? current_node->children : children;

				if (pos == end)
				{
					/* File or directory leaf */
					auto j = c.insert (make_pair (sub, FileTrieNode<T> (current_node, true, sub)));
					current_node = &j.first->second;
				}
				else
				{
					/* Inner directory */
					auto j = c.insert (make_pair (sub, FileTrieNode<T> (current_node, false, sub)));
					current_node = &j.first->second;
				}
			}


			begin = pos + 1;
			if (begin > end)
				break;
		}
	}

	/* Trailing slash marks directory */
	FileTrieNodeHandle<T> find_element (const std::string& _path)
	{
		/* Elliminate double slashes */
		std::string path = simplify_path ('/' + _path);

		std::string::size_type begin = 0;
		std::string::size_type end = path.size();

		FileTrieNode<T> *current_node = nullptr;

		for (;;)
		{
			std::string::size_type pos;
			std::string sub;

			/* Another component but we saw a leaf already? - abort. */
			if (current_node && current_node->is_leaf)
				return FileTrieNodeHandle<T>();

			if (begin < end)
			{
				pos = path.find ('/', begin);

				if (pos == 0)
				{
					begin = 1;
					continue;
				}

				if (pos == std::string::npos)
					pos = end;

				sub = path.substr (begin, pos - begin);
			}
			else
			{
				pos = end;
				sub = "";
			}


			if (current_node)
			{
				auto i = current_node->children.find (sub);

				if (i != current_node->children.end())
					current_node = &i->second;
				else
					return FileTrieNodeHandle<T>();
			}
			else
			{
				auto i = children.find (sub);

				if (i != children.end())
					current_node = &i->second;
				else
					return FileTrieNodeHandle<T>();
			}


			begin = pos + 1;
			if (begin > end)
			{
				/* We'll always have a current node when we come here. */
				if (current_node->is_leaf)
					return FileTrieNodeHandle<T>(current_node);
				else
					return FileTrieNodeHandle<T>();
			}
		}
	}


public:
	/* Only inserts the specified element if it does not exist already. Even if
	 * the existing one is of the other type. */
	void insert_file (const std::string& path)
	{
		if (path.size() == 0 || path.back() == '/')
			return;

		insert_element (path);
	}

	void insert_directory (const std::string& path)
	{
		insert_element (path + '/');
	}


	/* Return an invalid handle (false when converted to bool) if the element is
	 * not in the trie. */
	FileTrieNodeHandle<T> find_file (const std::string& path)
	{
		if (path.size() == 0 || path.back() == '/')
			return FileTrieNodeHandle<T>();

		return find_element (path);
	}

	FileTrieNodeHandle<T> find_directory (const std::string& path)
	{
		return find_element (path + '/');
	}


	/* @returns true if the element was removed. Directories are not removed
	 * recursively as this should act more like a filesystem object store where
	 * each element has a path, and not like a real filesystem. */
	bool remove_element (const std::string& _path)
	{
		/* Elliminate double slashes */
		std::string path = simplify_path ('/' + _path);

		std::string::size_type begin = 0;
		std::string::size_type end = path.size();

		FileTrieNode<T> *current_node = nullptr;

		for (;;)
		{
			std::string::size_type pos = path.find ('/', begin);

			if (pos == 0)
			{
				begin = 1;
				continue;
			}

			if (pos == std::string::npos)
				pos = end;

			const std::string& sub = path.substr (begin, pos - begin);


			auto i = current_node ? current_node->children.find (sub) : children.find (sub);
			auto children_end = current_node ? current_node->children.end() : children.end();

			if (i != children_end)
				current_node = &i->second;
			else
				return false;


			begin = pos + 1;
			if (begin >= end)
			{
				if (current_node->is_leaf)
				{
					auto parent = current_node->parent;

					if (parent)
						parent->children.erase (sub);
					else
						children.erase (sub);

					current_node = parent;

					break;
				}
				else
				{
					i = current_node->children.find ("");
					if (i != children_end)
					{
						current_node->children.erase (i);
						break;
					}
					else
					{
						return false;
					}
				}
			}
		}

		/* Current node is the directory in which content has been deleted, or
		 * nullptr if it's the root. Check for empty directories (those that
		 * don't even contain directory dummy leaves) and delete them. */
		while (current_node)
		{
			auto parent = current_node->parent;

			if (current_node->children.size() == 0)
			{
				if (parent)
					parent->children.erase (current_node->name);
				else
					children.erase (current_node->name);
			}
			else
			{
				break;
			}

			current_node = parent;
		}

		return true;
	}
};


template<typename T>
class FileTrieNode
{
	friend FileTrie<T>;
	friend FileTrieTestAdaptor<T>;

private:
	std::map<std::string, FileTrieNode<T>> children;
	FileTrieNode<T> *parent;
	bool is_leaf;

	std::string name;

	FileTrieNode (FileTrieNode<T> *parent, bool is_leaf, const std::string& name)
		: parent(parent), is_leaf(is_leaf), name(name), data{}
	{
	}


public:
	T data;

	std::string get_path() const
	{
		std::string p = name;

		if (p.size() > 0)
			p = "/" + p;

		if (parent)
		{
			p = parent->get_path() + p;
		}
		else
		{
			/* The root directory would actually have the path "", but unix
			 * software seems to agree that "/" (with a trailing slash) is
			 * better in that case. */
			if (p.size() == 0)
				return "/";
		}

		return p;
	}
};


template<typename T>
class FileTrieTestAdaptor
{
public:
	static std::map<std::string, FileTrieNode<T>> &get_children (FileTrie<T>& trie)
	{
		return trie.children;
	}

	static std::map<std::string, FileTrieNode<T>> &get_children (FileTrieNode<T>& node)
	{
		return node.children;
	}

	static FileTrieNode<T> *get_parent (FileTrieNode<T>& node)
	{
		return node.parent;
	}

	static bool get_is_leaf (FileTrieNode<T>& node)
	{
		return node.is_leaf;
	}

	static std::string& get_name (FileTrieNode<T>& node)
	{
		return node.name;
	}

	static FileTrieNode<T> *get_node_pointer (FileTrieNodeHandle<T> &h)
	{
		return h.ptr;
	}

	static FileTrieNode<T> make_node ()
	{
		return FileTrieNode<T>(nullptr, true, "");
	}

	static FileTrieNodeHandle<T> make_handle ()
	{
		return FileTrieNodeHandle<T>();
	}

	static FileTrieNodeHandle<T> make_handle (FileTrieNode<T> *n)
	{
		return FileTrieNodeHandle<T>(n);
	}
};

#endif /* __FILE_TRIE_H */
