
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


extern int u_switchIndex;
extern std::queue<std::vector<BYTE>>list1;
extern std::queue<std::vector<BYTE>>list2;
extern CRITICAL_SECTION m_cs1;
extern CRITICAL_SECTION m_cs2;
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
	ON_COMMAND(ID_VIEW_TOGGLEPDUHEX, &CBusWinApplView::OnViewTogglepduhex)
	ON_COMMAND(ID_VIEW_TOGGLEIDHEX, &CBusWinApplView::OnViewToggleidhex)
END_MESSAGE_MAP()

// CBusWinApplView construction/destruction

CBusWinApplView::CBusWinApplView() noexcept
{
	// TODO: add construction code here
	bPDUhexView = true;
	bIdViweHex = true;

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

std::wstring CBusWinApplView::ByteTohexStr(BYTE mybyte)
{
	wchar_t buffer[3];
	if ((mybyte >= 0) && (mybyte <= 15))
	{
		std::swprintf(buffer, 3, L"0%x\0", mybyte);
	}
	else
	{
		std::swprintf(buffer, 3, L"%x\0", mybyte);
	}
	std::wstring localstr = buffer;
	std::transform(localstr.begin(), localstr.end(), localstr.begin(), ::toupper);
	return localstr;
}
std::wstring CBusWinApplView::IntToStr(WORD myword)
{
	wchar_t bufferlower[3];
	wchar_t bufferhigh[3];
	BYTE mylowerbyte;
	BYTE myhighByte;
	mylowerbyte = myword & 0x00FF;
	myhighByte = (myword & 0xFF00) >> 8;
	if ((mylowerbyte >= 0) && (mylowerbyte <= 15))
	{
		std::swprintf(bufferlower, 3, L"0%x\0", mylowerbyte);
	}
	else
	{
		std::swprintf(bufferlower, 3, L"%x\0", mylowerbyte);
	}
	if ((myhighByte >= 0) && (myhighByte <= 15))
	{
		std::swprintf(bufferhigh, 3, L"0%x\0", myhighByte);
	}
	else
	{
		std::swprintf(bufferhigh, 3, L"%x\0", myhighByte);
	}
	std::wstring localstrlower = bufferlower;
	std::wstring localstrhigh = bufferhigh;
	std::wstring localstr = localstrhigh + localstrlower;
	std::transform(localstr.begin(), localstr.end(), localstr.begin(), ::toupper);
	return localstr;
}

std::wstring CBusWinApplView::longIntToStr(DWORD Dmyword)
{
	DWORD localvar;
	localvar = ((DWORD)Dmyword & 0xFFFF0000) >> 16;
	std::wstring localhi = IntToStr((WORD)localvar);
	localvar = ((DWORD)Dmyword) & ((DWORD)0x0000FFFF);
	std::wstring locallo = IntToStr((WORD)localvar);
	std::wstring localstr = localhi + locallo;
	return localstr;
}

// CBusWinApplView message handlers

void CBusWinApplView::TransformBuffer(LPCTSTR &mystr, std::vector<BYTE> localBuffur)
{
	LPCTSTR  pch = nullptr;
	LPCTSTR  pch1 = nullptr;
	pch = new TCHAR[1000];
	std::wstring mynewStr(_T(""));
	std::wstring emptystr(_T(" "));
	std::wstring douapuncte(_T(":"));
	std::wstring delimitator(_T("->"));
	BYTE myhour;
	BYTE myminutes;
	BYTE myseconds;
	DWORD myCANId;
	DWORD myTimeStampUs;
	WORD mypdulength;
	DWORD DeltaTimeStampUs;

	myhour = localBuffur[localBuffur.size()-1];
	myminutes = localBuffur[localBuffur.size()-2];
	myseconds = localBuffur[localBuffur.size()-3]; 
	std::wstring mylocalstr;
	mylocalstr = std::to_wstring(myhour);
	mynewStr = mynewStr + mylocalstr;
	mylocalstr = std::to_wstring(myminutes);
	mynewStr = mynewStr + douapuncte + mylocalstr;
	mylocalstr = std::to_wstring(myseconds);
	mynewStr = mynewStr + douapuncte + mylocalstr ;
	myCANId = localBuffur[0] + localBuffur[1] * 256 + localBuffur[2] * 65536 + localBuffur[3]* 16777216;
	mypdulength = localBuffur[4] + localBuffur[5] * 256;
	
	myTimeStampUs = localBuffur[6] + localBuffur[7] * 256 + localBuffur[8] * 65536 + localBuffur[9] * 16777216;
	DeltaTimeStampUs = myTimeStampUs - oldTimeStamp;
	oldTimeStamp = myTimeStampUs;
	mynewStr = mynewStr + douapuncte+ std::to_wstring(DeltaTimeStampUs) + delimitator;
	if (bIdViweHex)
	{
		mynewStr = mynewStr + emptystr + longIntToStr(myCANId);
	}
	else
	{
		mynewStr = mynewStr + emptystr + std::to_wstring(myCANId);
	}
	for (WORD ui16LocalIndex = 10; ui16LocalIndex<(10 + mypdulength); ui16LocalIndex++)
	{
		std::wstring mylocalstr;
		BYTE var = localBuffur[ui16LocalIndex];
		if (bPDUhexView)
		{
			mylocalstr = ByteTohexStr(var);
		}
		else
		{
			mylocalstr = std::to_wstring(var);
		}
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
			EnterCriticalSection(&m_cs1);
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
			LeaveCriticalSection(&m_cs1);
		}
		else
		{
			/*second buffer implementation*/
			u_switchIndex = 0;
			EnterCriticalSection(&m_cs2);
			while (!list2.empty())
			{
				tempbuffer = list2.front();
				list2.pop();
				TransformBuffer(lpstring, tempbuffer);
				GetTreeCtrl().InsertItem(lpstring, 1, 1, hCAN1tree);
				if (lpstring != nullptr) delete[]lpstring;
			}
			LeaveCriticalSection(&m_cs2);
		}
		
	}
	CTreeView::OnTimer(nIDEvent);
}


void CBusWinApplView::OnViewTogglepduhex()
{
	if (bPDUhexView)
	{
		bPDUhexView = false;
	}
	else
	{
		bPDUhexView = true;
	}
	// TODO: Add your command handler code here
}


void CBusWinApplView::OnViewToggleidhex()
{
	if (bIdViweHex)
	{
		bIdViweHex = false;
	}
	else
	{
		bIdViweHex = true;
	}
	// TODO: Add your command handler code here
}
