
// BusWinApplView.cpp : implementation of the CBusWinApplView class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "BusWinAppl.h"
#endif

#include "BusWinApplDoc.h"
#include "BusWinApplView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <vector>
#include <queue>
#include <string>
extern int u_switchIndex;
extern std::queue<std::vector<BYTE>>list1;
extern std::queue<std::vector<BYTE>>list2;
// CBusWinApplView

IMPLEMENT_DYNCREATE(CBusWinApplView, CTreeView)

BEGIN_MESSAGE_MAP(CBusWinApplView, CTreeView)
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, &CTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CBusWinApplView::OnFilePrintPreview)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
	ON_WM_TIMER()
END_MESSAGE_MAP()

// CBusWinApplView construction/destruction

CBusWinApplView::CBusWinApplView() noexcept
{
	// TODO: add construction code here

}

CBusWinApplView::~CBusWinApplView()
{
}

BOOL CBusWinApplView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	if (!CTreeView::PreCreateWindow(cs))
		return FALSE;
	cs.style |= TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;
	return TRUE;
}

// CBusWinApplView drawing

void CBusWinApplView::OnDraw(CDC* /*pDC*/)
{
	CBusWinApplDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: add draw code for native data here
}


// CBusWinApplView printing


void CBusWinApplView::OnFilePrintPreview()
{
#ifndef SHARED_HANDLERS
	AFXPrintPreview(this);
#endif
}

BOOL CBusWinApplView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// default preparation
	return DoPreparePrinting(pInfo);
}

void CBusWinApplView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add extra initialization before printing
}

void CBusWinApplView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add cleanup after printing
}

void CBusWinApplView::OnInitialUpdate()
{
	CTreeView::OnInitialUpdate();

	//
	// Initialize the image list.
	//
	m_ilLinii.Create(IDB_LINIIIMAGES, 16, 1, RGB(255, 0, 255));
	GetTreeCtrl().SetImageList(&m_ilLinii, TVSIL_NORMAL);

	//
	// Populate the tree view with drive items.
	//
	// Root items first, with automatic sorting
	hCAN1tree = GetTreeCtrl().InsertItem(_T("CAN 1"),0, 0, TVI_ROOT, TVI_SORT);
	hCAN2tree = GetTreeCtrl().InsertItem(_T("CAN 2"),0, 0, TVI_ROOT, TVI_SORT);
	// Eagles subitems second (no sorting)
	GetTreeCtrl().InsertItem(_T("Initial string CAN1"), 1, 1, hCAN1tree);
	// Doobie subitems third (no sorting)
	GetTreeCtrl().InsertItem(_T("Initial string CAN2"), 1, 1, hCAN2tree);
	SetTimer(1, 500, NULL);
}


void CBusWinApplView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CBusWinApplView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CBusWinApplView diagnostics

#ifdef _DEBUG
void CBusWinApplView::AssertValid() const
{
	CView::AssertValid();
}

void CBusWinApplView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CBusWinApplDoc* CBusWinApplView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CBusWinApplDoc)));
	return (CBusWinApplDoc*)m_pDocument;
}
#endif //_DEBUG


// CBusWinApplView message handlers

void CBusWinApplView::TransformBuffer(LPCTSTR &mystr, std::vector<BYTE> localBuffur)
{
	LPCTSTR  pch = nullptr;
	LPCTSTR  pch1 = nullptr;
	pch = new TCHAR[1000];
	std::wstring mynewStr(_T(""));
	std::wstring emptystr(_T(" "));
	for each (BYTE var in localBuffur)
	{
		std::wstring mylocalstr;
		mylocalstr = std::to_wstring(var);
		mynewStr = mynewStr + emptystr+ mylocalstr;
	}
	pch1 = mynewStr.c_str();
	pch = _wcsdup(pch1);
	mystr = pch;
}
void CBusWinApplView::OnTimer(UINT_PTR nIDEvent)
{
	/*if timer expired the copy data aquared by thread to Ctreectl*/
	std::vector<BYTE> tempbuffer;
	LPCTSTR lpstring=nullptr;
	/*if the timer 1 event*/
	if (nIDEvent == 1)
	{
		/*switch buffer to evercome the concurency*/
		if (u_switchIndex == 0)
		{
			u_switchIndex = 1;
			/*empty queue to treecontrol*/
			while (!list1.empty())
			{
				tempbuffer = list1.front();
				list1.pop();
				/*convert number to string*/
				TransformBuffer(lpstring, tempbuffer);
				GetTreeCtrl().InsertItem(lpstring, 1, 1, hCAN1tree);
				/*delete alocated memory*/
				if (lpstring !=nullptr) delete[]lpstring;
				
			}
		}
		else
		{
			/*second buffer implementation*/
			u_switchIndex = 0;
			while (!list2.empty())
			{
				tempbuffer = list2.front();
				list2.pop();
				TransformBuffer(lpstring, tempbuffer);
				GetTreeCtrl().InsertItem(lpstring, 1, 1, hCAN1tree);
				if (lpstring != nullptr) delete[]lpstring;
			}
		}
	}
	CTreeView::OnTimer(nIDEvent);
}
