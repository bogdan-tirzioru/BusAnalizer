
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


// CBusWinApplView

IMPLEMENT_DYNCREATE(CBusWinApplView, CTreeView)

BEGIN_MESSAGE_MAP(CBusWinApplView, CTreeView)
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, &CTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CTreeView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CBusWinApplView::OnFilePrintPreview)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
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
	HTREEITEM hEagles = GetTreeCtrl().InsertItem(_T("CAN 1"),0, 0, TVI_ROOT, TVI_SORT);
	HTREEITEM hDoobies = GetTreeCtrl().InsertItem(_T("CAN 2"),0, 0, TVI_ROOT, TVI_SORT);

	// Eagles subitems second (no sorting)
	GetTreeCtrl().InsertItem(_T("01 23 34 56 76 89 11"), 1, 1, hEagles);
	GetTreeCtrl().InsertItem(_T("01 23 34 56 76 89 11"), 1, 1, hEagles);
	GetTreeCtrl().InsertItem(_T("01 23 34 56 76 89 11"), 1, 1, hEagles);
	GetTreeCtrl().InsertItem(_T("01 23 34 56 76 89 11"), 1, 1, hEagles);

	// Doobie subitems third (no sorting)
	GetTreeCtrl().InsertItem(_T("01 23 34 56 76 89 11"), 1, 1, hDoobies);
	GetTreeCtrl().InsertItem(_T("01 23 34 56 76 89 11"), 1, 1, hDoobies);
	GetTreeCtrl().InsertItem(_T("01 23 34 56 76 89 11"), 1, 1, hDoobies);
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
