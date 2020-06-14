
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

IMPLEMENT_DYNCREATE(CBusWinApplView, CView)

BEGIN_MESSAGE_MAP(CBusWinApplView, CView)
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
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

	return CView::PreCreateWindow(cs);
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
