// foo_mlsync.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <locale>
#include <codecvt>
#include <string>
#include <regex>

DECLARE_COMPONENT_VERSION("Sync Media Library Viewer with Now Playing", "0.9.0", "See ...");

VALIDATE_COMPONENT_FILENAME("foo_mlsync.dll");

#if 0
// Sample initquit implementation. See also: initquit class documentation in relevant header.
class myinitquit : public initquit {
public:
	void on_init() {
		console::print("Sample component: on_init()");
	}
	void on_quit() {
		console::print("Sample component: on_quit()");
	}
};

static initquit_factory_t<myinitquit> g_myinitquit_factory;
#endif

//////////////////////////////////////////
static const GUID guid_foo_mlsync_mainmenu_group = { 0x44963e7a, 0x4b2a, 0x4588,{ 0xb0, 0x17, 0xa8, 0x69, 0x18, 0xcb, 0x8a, 0xa6 } };
static HWND g_treeWindow;
static HTREEITEM g_treeItem;

class contextmenu_runaction : public contextmenu_item_simple {

public:
	/* Virtuals */
	virtual contextmenu_item_node::t_type get_type() { return contextmenu_item_node::TYPE_COMMAND; }
	virtual t_size get_children_count() { return 0; }
	virtual unsigned get_num_items() { return 1; }
	virtual contextmenu_item_node * get_child(t_size) { return NULL; }
	virtual void get_item_name(unsigned p_index, pfc::string_base & p_out) {
		PFC_ASSERT(p_index < get_num_items());
		switch (p_index) {
		case 0:
			p_out = "Show in Media Library Viewer"; break;
		}
	}
	virtual bool get_item_description(unsigned p_index, pfc::string_base & p_out) {
		PFC_ASSERT(p_index < get_num_items());
		switch (p_index) {
		case 0:
			p_out = "Show in Media Library Viewer.";
			return true;
		default:
			PFC_ASSERT(!"Should not get here");
			return false;
		}
	}
	virtual GUID get_item_guid(unsigned p_index) {
		static const GUID myguid = { 0xc6929345,0xb1f6,0x44a7,{0x85,0x8d,0x02,0xf8,0x2d,0x8e,0x60,0xf7} };
		if (p_index == 0) {
			return myguid;
		}
		return pfc::guid_null;
	}
	void context_command(unsigned p_index, metadb_handle_list_cref p_data, const GUID& p_caller) {
		PFC_ASSERT(p_index < get_num_items());

		static_api_ptr_t<play_control> pc;
		metadb_handle_ptr ptr = { 0 };
		if (pc->get_now_playing(ptr))
		{
			auto api = library_manager::get();
			pfc::string8 relPath8;
			api->get_relative_path(ptr, relPath8);
			// console::printf("NowPlaying GetItemPath = [%s] => [%s] ptr=[%x]", ptr->get_path(), relPath8.c_str(), (long)(ptr.get_ptr()));

			titleformat_object::ptr script;
			titleformat_compiler::get()->compile_force(script, "[%album%]");
			pfc::string8 album;
			ptr->format_title(NULL, album, script, NULL);

			titleformat_compiler::get()->compile_force(script, "[%artist%]");
			pfc::string8 artist;
			ptr->format_title(NULL, artist, script, NULL);
			// console::printf("  Album[%s] Artist [%s]", album.c_str(), artist.c_str());

			if (ptr->get_info_ref()->info().meta_exists("date")) {
				titleformat_compiler::get()->compile_force(script, "[%artist%] - '['[%date%]']' [%album%]");
			}
			else {
				titleformat_compiler::get()->compile_force(script, "[%artist%] - [%album%]");
			}
			pfc::string8 artist_album;
			ptr->format_title(NULL, artist_album, script, NULL);
			// console::printf("  Artist_Album[%s]", artist_album.c_str());

			HWND hwndMain = core_api::get_main_window();

			// Locate via "by album"
			g_treeWindow = 0;
			g_treeItem = 0;
			::EnumChildWindows(hwndMain, MyEnumChildProc, (LPARAM)album.c_str());
			// console::printf("1. Found Tree = %x item=[%x]", g_treeWindow, g_treeItem);
			if (g_treeWindow && g_treeItem) {
				TreeView_SelectItem(g_treeWindow, g_treeItem);
				TreeView_EnsureVisible(g_treeWindow, g_treeItem);
				return;
			}

			// Locate via "by artist"
			g_treeWindow = 0;
			g_treeItem = 0;
			::EnumChildWindows(hwndMain, MyEnumChildProc, (LPARAM)artist.c_str());
			// console::printf("2a. Found Tree = %x item=[%x]", g_treeWindow, g_treeItem);
			if (g_treeWindow && g_treeItem) {
				TreeView_SelectItem(g_treeWindow, g_treeItem);
				TreeView_EnsureVisible(g_treeWindow, g_treeItem);
				TreeView_Expand(g_treeWindow, g_treeItem, TVM_EXPAND);

				g_treeWindow = 0;
				g_treeItem = 0;
				::EnumChildWindows(hwndMain, MyEnumChildProc, (LPARAM)album.c_str());
				// console::printf("2b. Found Tree = %x item=[%x]", g_treeWindow, g_treeItem);
				if (g_treeWindow && g_treeItem) {
					TreeView_SelectItem(g_treeWindow, g_treeItem);
					TreeView_EnsureVisible(g_treeWindow, g_treeItem);
					return;
				}
			}

			// Locate via "by artist/album"
			g_treeWindow = 0;
			g_treeItem = 0;
			::EnumChildWindows(hwndMain, MyEnumChildProc, (LPARAM)artist_album.c_str());
			// console::printf("2A. Found Tree = %x item=[%x]", g_treeWindow, g_treeItem);
			if (g_treeWindow && g_treeItem) {
				TreeView_SelectItem(g_treeWindow, g_treeItem);
				TreeView_EnsureVisible(g_treeWindow, g_treeItem);
				return;
			}

			// Locate via "by folder structure"
			std::string relPath(relPath8.c_str());
			std::regex rgx("\\\\");
			std::sregex_token_iterator iter(relPath.begin(), relPath.end(), rgx, -1);
			std::sregex_token_iterator end;
			for (; iter != end; ++iter) {
				std::string relPathComponent(*iter);
				// console::printf("3a. Found Compnent=[%s]", relPathComponent.c_str());
				g_treeWindow = 0;
				g_treeItem = 0;
				::EnumChildWindows(hwndMain, MyEnumChildProc, (LPARAM)relPathComponent.c_str());
				if (g_treeWindow && g_treeItem) {
					TreeView_SelectItem(g_treeWindow, g_treeItem);
					TreeView_EnsureVisible(g_treeWindow, g_treeItem);
					TreeView_Expand(g_treeWindow, g_treeItem, TVM_EXPAND);
				}
			}
			if (g_treeWindow && g_treeItem) {
				// Success
				return;
			}
		}
	}
	static BOOL CALLBACK MyEnumChildProc(_In_ HWND hwnd, _In_ LPARAM lParam)
	{
		char className[_MAX_PATH] = { 0 };
		::GetClassNameA(hwnd, className, _MAX_PATH);
		char title[_MAX_PATH] = { 0 };
		::GetWindowTextA(hwnd, title, sizeof(title));
		if (!strcmp(className, "SysTreeView32") && !strcmp(title, "Tree1")) {
			// console::printf("Found = %x class=[%s] title=[%s]", (int)hwnd, className, title);
			GetChildItems(hwnd, 0, (const char *)lParam);
			g_treeWindow = hwnd;
			return FALSE;
		}
		return TRUE;
	}
private:

	static void GetChildItems(const HWND tree, HTREEITEM startItem = NULL, const char *label = "none")
	{
		if (startItem == NULL) {
			startItem = TreeView_GetRoot(tree);
		}
		for (HTREEITEM item = startItem; item != NULL; item = TreeView_GetNextItem(tree, item, TVGN_NEXT))
		{
			// figure out if this item 
			wchar_t buf[100];
			memset(buf, 0, sizeof(buf));
			TVITEM tvitem = { 0 };
			tvitem.hItem = item;
			tvitem.cchTextMax = 100;
			tvitem.pszText = buf;
			tvitem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_HANDLE;
			TreeView_GetItem(tree, &tvitem);
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			std::string ss(converter.to_bytes(tvitem.pszText).c_str());
			int pos = ss.rfind(" (");
			bool endswith = ss.rfind(")") == (ss.size() - 1);
			if (endswith && pos >= 0)
			{
				ss = ss.substr(0, pos);
			}
			pos = ss.rfind(" [");
			endswith = ss.rfind("]") == (ss.size() - 1);
			if (endswith && pos >= 0)
			{
				ss = ss.substr(0, pos);
			}
			// TODO: Handle non-ascii characters - they are stripped out in TreeView_GetItem
			if (label && !strcmp(ss.c_str(), label))
			{
				// console::printf("Found Item ==> [%s]", ss.c_str());
				g_treeItem = item;
				return;
			}

			// deal with children if present
			HTREEITEM child = TreeView_GetNextItem(tree, item, TVGN_CHILD);
			if (child != NULL) {
				GetChildItems(tree, child, label);
			}
		}
	}
};

static contextmenu_item_factory_t< contextmenu_runaction > g_contextmenu_runaction;
