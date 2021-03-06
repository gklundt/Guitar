
#include "FileUtil.h"
#include "common/joinpath.h"

#include <sys/stat.h>

#ifdef __HAIKU__
struct stat	s;
#endif

#ifdef _WIN32
#include <windows.h>
#include <direct.h>

std::string FileUtil::getcwd()
{
	std::string cwd;
	char *p = _getcwd(0, 0);
	cwd = p;
	free(p);
	return cwd;
}

bool FileUtil::isdir(std::string const &path)
{
	DWORD f = GetFileAttributesA(path.c_str());
	return f != INVALID_FILE_ATTRIBUTES && (f & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void FileUtil::mkdir(const std::string &dir, int /*perm*/)
{
	CreateDirectoryA(dir.c_str(), 0);
}

void FileUtil::rmdir(const std::string &dir)
{
	RemoveDirectoryA(dir.c_str());
}

bool FileUtil::chdir(const std::string &dir)
{
	return SetCurrentDirectoryA(dir.c_str()) != 0;
}

void FileUtil::rmfile(const std::string &path)
{
	DeleteFileA(path.c_str());
}

void FileUtil::mv(const std::string &src, const std::string &dst)
{
	MoveFileA(src.c_str(), dst.c_str());
//	DWORD e = GetLastError();
}

void FileUtil::getdirents(const std::string &loc, std::vector<DirEnt> *out)
{
	out->clear();
	std::string filter = loc / "*.*";
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(filter.c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			DirEnt de;
			de.name = fd.cFileName;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (de.name == "." || de.name == "..") {
					continue;
				}
				de.isdir = true;
			}
			out->push_back(de);
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
}

#else // _WIN32
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>

std::string FileUtil::getcwd()
{
	std::string cwd;
	char *p = ::getcwd(0, 0);
	cwd = p;
	free(p);
	return cwd;
}

bool FileUtil::isdir(std::string const &path)
{
	struct stat st;
	if (stat(path.c_str(), &st) == 0) {
		if (st.st_mode & S_IFDIR) {
			return true;
		}
	}
	return false;
}

void FileUtil::rmdir(const std::string &dir)
{
	::rmdir(dir.c_str());
}

void FileUtil::rmfile(const std::string &path)
{
	unlink(path.c_str());
}

void FileUtil::mkdir(const std::string &dir, int perm)
{
	::mkdir(dir.c_str(), perm);
}

bool FileUtil::chdir(const std::string &dir)
{
	return ::chdir(dir.c_str()) == 0;
}

void FileUtil::mv(std::string const &src, std::string const &dst)
{
	rename(src.c_str(), dst.c_str());
}

void FileUtil::getdirents(std::string const &loc, std::vector<DirEnt> *out)
{
	out->clear();
	DIR *dir = opendir(loc.c_str());
	if (dir) {
		while (dirent *d = readdir(dir)) {
			DirEnt de;
			de.name = d->d_name;
			de.isdir = false;
			#ifdef DT_DIR
				if (d->d_type & DT_DIR) {
			#else
				stat(d->d_name, &s);
				if(S_ISDIR(s.st_mode))  {
			#endif
				if (de.name == "." || de.name == "..") {
					continue;
				}
				de.isdir = true;
			}
			out->push_back(de);
		}
		closedir(dir);
	}
}

#endif

void FileUtil::rmtree(const std::string &path)
{
	std::vector<DirEnt> ents;
	getdirents(path, &ents);
	for (DirEnt const &e : ents) {
		if (e.isdir) {
			rmtree(path / e.name);
		} else {
			rmfile(path / e.name);
		}
	}
	rmdir(path);
}

#ifdef _WIN32
std::string FileUtil::normalize_path_separator(std::string const &s)
{
	if (!s.empty()) {
		size_t n = s.size();
		std::vector<char> v;
		v.reserve(n);
		for (size_t i = 0; i < n; i++) {
			char c = s[i];
			if (c == '/') {
				c = '\\';
			}
			v.push_back(c);
		}
		char const *p = &v[0];
		return std::string(p, p + n);
	}
	return std::string();
}
#else
std::string FileUtil::normalize_path_separator(std::string const &s)
{
	return s;
}
#endif

static std::string which(std::string const &name, std::string const &dir)
{
	std::vector<FileUtil::DirEnt> ents;
	FileUtil::getdirents(dir, &ents);
	for (FileUtil::DirEnt const &ent : ents) {
		if (!ent.isdir) {
#ifdef _WIN32
			if (stricmp(ent.name.c_str(), name.c_str()) == 0) {
				return FileUtil::normalize_path_separator(dir / ent.name);
			}
#else
			if (ent.name == name) {
				return dir / ent.name;
			}
#endif
		}
	}
	return std::string();
}

static void split_path(std::string const &path, std::vector<std::string> *out)
{
	char const *begin = path.c_str();
	char const *end = begin + path.size();
	char const *ptr = begin;
	char const *left = ptr;
#ifdef _WIN32
	const int sep = ';';
#else
	const int sep = ':';
#endif
	while (1) {
		int c = 0;
		if (ptr < end) {
			c = *ptr & 0xff;
		}
		if (c == sep || c == 0) {
			if (left < ptr) {
				out->push_back(std::string(left, ptr));
			}
			if (c == 0) break;
			ptr++;
			left = ptr;
		} else {
			ptr++;
		}
	}
}

std::string FileUtil::which(std::string const &name, std::vector<std::string> *out)
{
	if (out) {
		out->clear();
	}
	char const *p = getenv("PATH");
	if (p) {
		std::vector<std::string> paths;
		split_path(p, &paths);
		for (std::string const &path : paths) {
			std::string t = ::which(name, path);
			if (!t.empty()) {
				if (out) {
					out->push_back(t);
				} else {
					return t;
				}
			}
		}
	}
	if (out && !out->empty()) {
		return out->front();
	}
	return std::string();
}

