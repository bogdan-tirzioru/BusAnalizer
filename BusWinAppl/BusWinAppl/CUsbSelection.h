#pragma once


// CUsbSelection dialog

class CUsbSelection : public CDialogEx
{
	DECLARE_DYNAMIC(CUsbSelection)

public:
	CUsbSelection(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CUsbSelection();
// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_USB };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnConnectUsb();
	afx_msg void OnLbnSelchangeList1();
	CListBox m_listUsb;
	CString m_strListBox;
	void LoadListBox();
	BOOL OnInitDialog();
};
