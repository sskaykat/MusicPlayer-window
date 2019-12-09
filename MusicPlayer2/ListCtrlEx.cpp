#include "stdafx.h"
#include "ListCtrlEx.h"
#include "MusicPlayer2.h"
#include <set>

IMPLEMENT_DYNAMIC(CListCtrlEx, CListCtrl)

CListCtrlEx::CListCtrlEx()
	: m_theme_color(theApp.m_app_setting_data.theme_color)
{
	
	//初始化颜色
	//m_theme_color.original_color = GRAY(180);
	//CColorConvert::ConvertColor(m_theme_color);
    m_drag_cursor = AfxGetApp()->LoadCursor(IDC_DRAG_CURSOR);
}


CListCtrlEx::~CListCtrlEx()
{
}


//void CListCtrlEx::SetColor(const ColorTable & colors)
//{
//	m_theme_color = colors;
//	if (m_hWnd != NULL)
//		Invalidate();
//}

void CListCtrlEx::GetItemSelected(vector<int>& item_selected) const
{
	item_selected.clear();
	POSITION pos = GetFirstSelectedItemPosition();
	if (pos != NULL)
	{
		while (pos)
		{
			int nItem = GetNextSelectedItem(pos);
			item_selected.push_back(nItem);
		}
	}
}

int CListCtrlEx::GetCurSel() const
{
	vector<int> item_selected;
	GetItemSelected(item_selected);
	if (!item_selected.empty())
		return item_selected[0];
	else
		return -1;
}

void CListCtrlEx::SetCurSel(int select)
{
	int size = GetItemCount();
	if (select >= 0 && select < size)
	{
		SetItemState(select, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);	//选中行
		EnsureVisible(select, FALSE);		//使选中行保持可见
	}
	else
	{
		//取消所有行的选中
		for(int i{}; i<size; i++)
			SetItemState(i, 0, LVIS_SELECTED);
	}
}

void CListCtrlEx::SetCurSel(int first, int last)
{
    int itemCnt = GetItemCount();
    if (first >= 0 && last < itemCnt && first <= last)
    {
        for (int i = 0; i < itemCnt; i++)
        {
            if(i>=first && i<=last)
                SetItemState(i, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            else
                SetItemState(i, 0, LVIS_SELECTED);
        }
    }
}

void CListCtrlEx::SelectAll()
{
	int itemCnt = GetItemCount();
	for (int i = 0; i < itemCnt; i++)
	{
		SetItemState(i, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
	}
}

void CListCtrlEx::SelectNone()
{
    int itemCnt = GetItemCount();
    for (int i = 0; i < itemCnt; i++)
    {
        SetItemState(i, 0, LVIS_SELECTED);
    }
}

void CListCtrlEx::SelectReverse()
{
    std::vector<int> selected_vect;
    GetItemSelected(selected_vect);
    std::set<int> selected_set;
    for (auto n : selected_vect)
        selected_set.insert(n);
    int size = GetItemCount();
    for (int i = 0; i < size; i++)
    {
        if(selected_set.find(i)==selected_set.end())
            SetItemState(i, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
        else
            SetItemState(i, 0, LVIS_SELECTED);
    }
}

bool CListCtrlEx::SetRowHeight(int height)
{
	if (height > 0 && height <= 512)
	{
		CImageList imgList;		//为ClistCtrl设置一个图像列表，以设置行高
		BOOL rtn = imgList.Create(1, height, ILC_COLOR, 1, 1);
		if (rtn != FALSE)
		{
			SetImageList(&imgList, LVSIL_SMALL);
			return true;
		}
	}
	return false;
}


void CListCtrlEx::ShowPopupMenu(CMenu* pMenu, int item_index, CWnd* pWnd)
{
    CPoint point;			//定义一个用于确定光标位置的位置
    GetCursorPos(&point);	//获取当前光标的位置，以便使得菜单可以跟随光标
    if(item_index >= 0)
    {
        CRect item_rect;
        GetItemRect(item_index, item_rect, LVIR_BOUNDS);		//获取选中项目的矩形区域（以列表控件左上角为原点）
        CRect window_rect;
        GetWindowRect(window_rect);		//获取列表控件的矩形区域（以屏幕左上角为原点）
        point.y = window_rect.top + item_rect.bottom;	//设置鼠标要弹出的y坐标为选中项目的下边框位置，防止右键菜单挡住选中的项目
    }
    pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, pWnd); //在指定位置显示弹出菜单
}

BEGIN_MESSAGE_MAP(CListCtrlEx, CListCtrl)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CListCtrlEx::OnNMCustomdraw)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_SETFOCUS()
    ON_NOTIFY_REFLECT(LVN_BEGINDRAG, &CListCtrlEx::OnLvnBegindrag)
    ON_WM_LBUTTONUP()
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()


void CListCtrlEx::OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = CDRF_DODEFAULT;
	LPNMLVCUSTOMDRAW lplvdr = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
	NMCUSTOMDRAW& nmcd = lplvdr->nmcd;
	static bool this_item_select = false;
	switch (lplvdr->nmcd.dwDrawStage)	//判断状态   
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:			//如果为画ITEM之前就要进行颜色的改变
		this_item_select = false;
		if (IsWindowEnabled())
		{
			//当选中行又是高亮行时设置颜色
			if (GetItemState(nmcd.dwItemSpec, LVIS_SELECTED) == LVIS_SELECTED && nmcd.dwItemSpec == m_highlight_item)
			{
				this_item_select = true;
				SetItemState(nmcd.dwItemSpec, 0, LVIS_SELECTED);
				lplvdr->clrText = m_theme_color.light3;
				lplvdr->clrTextBk = m_theme_color.dark1;
			}
			//设置选中行的颜色
			else if (GetItemState(nmcd.dwItemSpec, LVIS_SELECTED) == LVIS_SELECTED)
			{
				this_item_select = true;
				SetItemState(nmcd.dwItemSpec, 0, LVIS_SELECTED);
				lplvdr->clrText = m_theme_color.dark3;
				lplvdr->clrTextBk = m_theme_color.light2;
			}
			//设置高亮行的颜色
			else if (nmcd.dwItemSpec == m_highlight_item)
			{
				lplvdr->clrText = m_theme_color.dark2;
				//lplvdr->clrText = 0;
				lplvdr->clrTextBk = m_theme_color.light3;
			}
			//设置偶数行的颜色
			else if (nmcd.dwItemSpec % 2 == 0)
			{
				lplvdr->clrText = CColorConvert::m_gray_color.dark3;
				lplvdr->clrTextBk = CColorConvert::m_gray_color.light3;
			}
			//设置奇数行的颜色
			else
			{
				lplvdr->clrText = CColorConvert::m_gray_color.dark3;
				lplvdr->clrTextBk = CColorConvert::m_gray_color.light4;
			}

			//用背景色填充单元格，以去掉每行前面的空白
			CRect rect = nmcd.rc;
			CDC* pDC = CDC::FromHandle(nmcd.hdc);		//获取绘图DC
			pDC->FillSolidRect(rect, lplvdr->clrTextBk);
		}
		else		//当控件被禁用时，显示文本设为灰色
		{
			lplvdr->clrText = GRAY(140);
			lplvdr->clrTextBk = GRAY(240);
		}
		*pResult = CDRF_NOTIFYPOSTPAINT;
		break;
	case CDDS_ITEMPOSTPAINT:
		if (this_item_select)
			SetItemState(nmcd.dwItemSpec, 0xFF, LVIS_SELECTED);
		//*pResult = CDRF_DODEFAULT;
		break;
	}
}


void CListCtrlEx::PreSubclassWindow()
{
	// TODO: 在此添加专用代码和/或调用基类
	CListCtrl::PreSubclassWindow();

    DWORD style = GetExtendedStyle();
    SetExtendedStyle(style | LVS_EX_DOUBLEBUFFER);      //设置双缓冲绘图

	SetBkColor(m_background_color);
	//SetHightItem(-1);
	SetRowHeight(theApp.DPI(22));
}


void CListCtrlEx::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	this->SetFocus();
    m_dragging = false;
    CListCtrl::OnLButtonDown(nFlags, point);
}


void CListCtrlEx::OnRButtonDown(UINT nFlags, CPoint point)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	this->SetFocus();
    m_dragging = false;
    CListCtrl::OnRButtonDown(nFlags, point);
}


BOOL CListCtrlEx::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 在此添加专用代码和/或调用基类

	//if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_CHAR)		//屏蔽列表控件的键盘消息，防止每次按下一个键时列表选中行会出现讨厌的、难看的虚线框
	//	return TRUE;

	return CListCtrl::PreTranslateMessage(pMsg);
}


void CListCtrlEx::OnSetFocus(CWnd* pOldWnd)
{
	CListCtrl::OnSetFocus(pOldWnd);

	// TODO: 在此处添加消息处理程序代码

	SendMessage(WM_KILLFOCUS);				//禁止列表控件获取焦点，防止选中行会出现难看的虚线框
}


void CListCtrlEx::OnLvnBegindrag(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    // TODO: 在此添加控件通知处理程序代码
    if (m_drag_enable)
    {
        m_dragging = true;
    }

    *pResult = 0;
}


void CListCtrlEx::OnLButtonUp(UINT nFlags, CPoint point)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (m_dragging)
    {
        CPoint pt(point);
        int drop_index = this->HitTest(pt);     //鼠标松开时的项目序号
        CWnd* pParent{ GetParent() };
        if (pParent != nullptr)
        {
            pParent->SendMessage(WM_LIST_ITEM_DRAGGED, drop_index, 0);       //结束拖放时向父窗口发送消息，传递拖放结束位置索引
        }
        m_dragging = false;
        SendMessage(WM_SETCURSOR);      //鼠标松开时刷新光标
    }

    CListCtrl::OnLButtonUp(nFlags, point);
}


BOOL CListCtrlEx::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (m_dragging)
    {
        ::SetCursor(m_drag_cursor);
        return TRUE;
    }

    return CListCtrl::OnSetCursor(pWnd, nHitTest, message);
}
