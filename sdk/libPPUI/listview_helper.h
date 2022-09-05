#pragma once

namespace listview_helper
{
	unsigned insert_item(HWND p_listview,unsigned p_index,const char * p_name,LPARAM p_param);//returns index of new item on success, infinite on failure

	unsigned insert_column(HWND p_listview,unsigned p_index,const char * p_name,unsigned p_width_dlu);//returns index of new item on success, infinite on failure

	bool set_item_text(HWND p_listview,unsigned p_index,unsigned p_column,const char * p_name);

	bool is_item_selected(HWND p_listview,unsigned p_index);

	void set_item_selection(HWND p_listview,unsigned p_index,bool p_state);

	bool select_single_item(HWND p_listview,unsigned p_index);

	bool ensure_visible(HWND p_listview,unsigned p_index);

	void get_item_text(HWND p_listview,unsigned p_index,unsigned p_column,pfc::string_base & p_out);

	unsigned insert_item2(HWND p_listview, unsigned p_index, const char * col0, const char * col1, LPARAM p_param = 0);
	unsigned insert_item3(HWND p_listview, unsigned p_index, const char * col0, const char * col1, const char * col2, LPARAM p_param = 0);


};

int ListView_GetFirstSelection(HWND p_listview);
int ListView_GetSingleSelection(HWND p_listview);
int ListView_GetFocusItem(HWND p_listview);
bool ListView_IsItemSelected(HWND p_listview, int p_index);

void ListView_GetContextMenuPoint(HWND p_list,LPARAM p_coords,POINT & p_point,int & p_selection);
void ListView_GetContextMenuPoint(HWND p_list,POINT p_coords,POINT & p_point,int & p_selection);

int ListView_GetColumnCount(HWND listView);

void ListView_FixContextMenuPoint(CListViewCtrl list, CPoint & coords);