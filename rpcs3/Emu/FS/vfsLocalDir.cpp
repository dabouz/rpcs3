#include "stdafx.h"
#include "vfsLocalDir.h"
#include <direct.h>

vfsLocalDir::vfsLocalDir(const wxString& path)
	: vfsDirBase(path)
	, m_pos(0)
{
}

vfsLocalDir::~vfsLocalDir()
{
}

bool vfsLocalDir::Open(const wxString& path)
{
	if(!vfsDirBase::Open(path))
		return false;

	wxDir dir;

	if(!dir.Open(path))
		return false;

	wxString name;
	for(bool is_ok = dir.GetFirst(&name); is_ok; is_ok = dir.GetNext(&name))
	{
		wxString dir_path = path + wxFILE_SEP_PATH + name;

		DirEntryInfo& info = m_entries[m_entries.Move(new DirEntryInfo())];
		info.name = name;

		info.flags |= wxDirExists(dir_path) ? DirEntry_TypeDir : DirEntry_TypeFile;
		if(wxIsWritable(dir_path)) info.flags |= DirEntry_PermWritable;
		if(wxIsReadable(dir_path)) info.flags |= DirEntry_PermReadable;
		if(wxIsExecutable(dir_path)) info.flags |= DirEntry_PermExecutable;
	}

	return true;
}

const DirEntryInfo* vfsLocalDir::Read()
{
	if (m_pos >= m_entries.GetCount())
		return 0;

	return &m_entries[m_pos++];
}

bool vfsLocalDir::Create(const wxString& path)
{
	return wxFileName::Mkdir(path, 0777, wxPATH_MKDIR_FULL);
}

bool vfsLocalDir::Rename(const wxString& from, const wxString& to)
{
	return false;
}

bool vfsLocalDir::Remove(const wxString& path)
{
	return wxRmdir(path);
}