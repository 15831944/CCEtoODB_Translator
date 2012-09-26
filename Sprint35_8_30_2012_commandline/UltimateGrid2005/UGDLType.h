// $Header: /CAMCAD/4.6/UltimateGrid2005/UGDLType.h 1     5/08/06 8:28p Kurt Van Ness $

/*************************************************************************
				Class Declaration : CUGDropListType
**************************************************************************
	Source file : UGDLType.cpp
	Header file : UGDLType.h
	Copyright � The Code Project 1994 - 2002, All Rights Reserved

	Purpose
		The CUGDropListType is the standard droplist cell type used
		in the Ultimate Grid.  This cell type uses the string stored
		in the 'label' portion of a given cell (set with SetLabelText)
		to populate the drop list.

		Note: In order to make sure that the items in the list are
		shown properly please make sure that each item is separated
		with a new line character (\n).

	Details
		Droplist extended styles
			UGCT_DROPLISTHIDEBUTTON	- the droplist button will only be shown
									  on current cells.  This style allows the
									  grid view not to be overly crowded with
									  droplist buttons keeping the view clean

		Droplist OnCellTypeNotify notifications
			UGCT_DROPLISTSTART		- this notification indicates that the user
									  decided to show the droplist.  This is the
									  final place where the items in the list
									  can be edited or added.
									  The 'param' parameter of this notification
									  contains a CStringList for the items to be
									  shown in the droplist. This list can be modified.
			UGCT_DROPLISTSELECT		- this notification indicates that the user
									  has selected an item in the droplist, 
									  in this notification you are presented with
									  the value that the user selected in the list.
									  The 'param' paramenter contains the a CString
									  pointer of the selected item.
			UGCT_DROPLISTSELECTEX	- this notification is identical to the
									  UGCT_DROPLISTSELECT with one exception, it
									  provides you with the index of the item selected.
									  The 'param' paramenter contains the index of 
									  the selected item.
			UGCT_DROPLISTPOSTSELECT - this notification will be sent after the user's
									  selection was successfuly saved in a cell.

*************************************************************************/
#ifndef _UGDLType_H_
#define _UGDLType_H_

// Droplist extended styles
#define UGCT_DROPLISTHIDEBUTTON	BIT9
// Droplist OnCellTypeNotify notifications
#define UGCT_DROPLISTSTART		100
#define UGCT_DROPLISTSELECT		101
#define UGCT_DROPLISTSELECTEX	102
#define UGCT_DROPLISTPOSTSELECT	103


//CUGDropListType declaration
class UG_CLASS_DECL CUGDropListType: public CUGCellType
{
protected:
	int		m_btnWidth;
	int		m_btnDown;
	RECT	m_btnRect;
	int		m_btnCol;
	long	m_btnRow;

	CPen*       m_pen;
	CBrush*     m_brush;
	CUGLstBox*	m_listBox;

	int StartDropList();

public:
	CUGDropListType();
	virtual ~CUGDropListType();

   virtual CPen& getPen();
   virtual CBrush& getBrush();

	virtual LPCTSTR GetName();
	virtual LPCUGID GetUGID();

	virtual int GetEditArea(RECT *rect);
	virtual void GetBestSize(CDC *dc,CSize *size,CUGCell *cell);
	virtual BOOL OnLClicked(int col,long row,int updn,RECT *rect,POINT *point);
	virtual BOOL OnDClicked(int col,long row,RECT *rect,POINT *point);
	virtual BOOL OnKeyDown(int col,long row,UINT *vcKey);
	virtual BOOL OnCharDown(int col,long row,UINT *vcKey);
	virtual BOOL OnMouseMove(int col,long row,POINT *point,UINT flags);
	virtual void OnKillFocus(int col,long row,CUGCell *cell);
	virtual void OnDraw(CDC *dc,RECT *rect,int col,long row,CUGCell *cell,int selected,int current);
   virtual COLORREF GetButtonColor();
};

#endif //#ifndef _UGDLType_H_