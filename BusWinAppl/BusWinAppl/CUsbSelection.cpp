// CUsbSelection.cpp : implementation file
//

#include "pch.h"
#include "BusWinAppl.h"
#include "CUsbSelection.h"
#include "afxdialogex.h"


// CUsbSelection dialog

IMPLEMENT_DYNAMIC(CUsbSelection, CDialogEx)

CUsbSelection::CUsbSelection(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_USB, pParent)
	, m_strListBox(_T(""))
{

}

CUsbSelection::~CUsbSelection()
{
}

void CUsbSelection::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_COM, m_listUsb);
	DDX_LBString(pDX, IDC_LIST_COM, m_strListBox);
}


BEGIN_MESSAGE_MAP(CUsbSelection, CDialogEx)
	ON_COMMAND(ID_CONNECT_USB, &CUsbSelection::OnConnectUsb)
	ON_LBN_SELCHANGE(IDC_LIST1, &CUsbSelection::OnLbnSelchangeList1)
END_MESSAGE_MAP()


// CUsbSelection message handlers


void CUsbSelection::OnConnectUsb()
{
	// TODO: Add your command handler code here
	AfxMessageBox(_T("Hello!"), MB_OK | MB_ICONINFORMATION);
}


void CUsbSelection::OnLbnSelchangeList1()
{
	// TODO: Add your control notification handler code here
	int nSel = m_listUsb.GetCurSel();
	if (nSel != LB_ERR)
	{
		
		m_listUsb.GetText(nSel, m_strListBox);

		//AfxMessageBox(m_strListBox);
	}
}

void CUsbSelection::LoadListBox() {
	CString str = _T("");
	for (int i = 0; i < 20; i++) {

		str.Format(_T("Com %d"), i);
		m_listUsb.AddString(str);
	}
}
BOOL CUsbSelection::OnInitDialog() {
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog. The framework does this automatically
	// when the application's main window is not a dialog
	//SetIcon(m_hIcon, TRUE);       // Set big icon
	//SetIcon(m_hIcon, FALSE);      // Set small icon

	// TODO: Add extra initialization here
	LoadListBox();
	return TRUE; // return TRUE unless you set the focus to a control
}