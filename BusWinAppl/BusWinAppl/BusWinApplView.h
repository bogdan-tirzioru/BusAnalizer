
// BusWinApplView.h : interface of the CBusWinApplView class
//

#pragma once
#include <vector>
#include <queue>
#include <string>

class CBusWinApplView : public CTreeView
{
protected: // create from serialization only
	CBusWinApplView() noexcept;
	DECLARE_DYNCREATE(CBusWinApplView)

// Attributes
public:
	CBusWinApplDoc* GetDocument() const;

// Operations
public:

// Overrides
public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnInitialUpdate(); //call imediatialy after the contructor

// Implementation
public:
	virtual ~CBusWinApplView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	CImageList m_ilLinii;
	HTREEITEM hCAN1tree;
	HTREEITEM hCAN2tree;
	DWORD oldTimeStamp;
	boolean bPDUhexView;
	boolean bIdViweHex;
	std::wstring CBusWinApplView::ByteTohexStr(BYTE mybyte);
	std::wstring CBusWinApplView::IntToStr(WORD myword);
	void TransformBuffer(LPCTSTR &mystr, std::vector<BYTE> localBuffur);
// Generated message map functions
protected:
	afx_msg void OnFilePrintPreview();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};

#ifndef _DEBUG  // debug version in BusWinApplView.cpp
inline CBusWinApplDoc* CBusWinApplView::GetDocument() const
   { return reinterpret_cast<CBusWinApplDoc*>(m_pDocument); }
#endif

