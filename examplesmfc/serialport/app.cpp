/////////////////////////////////  Includes  //////////////////////////////////

#include "stdafx.h"
#include "SerialPort.h"


//////////////////////////////// Implementation ///////////////////////////////

#pragma warning(suppress: 26461)
VOID WINAPI CompletionRoutine(_In_ DWORD dwErrorCode, _In_ DWORD dwNumberOfBytesTransfered, _Inout_ LPOVERLAPPED lpOverlapped) noexcept
{
  UNREFERENCED_PARAMETER(dwErrorCode);
  UNREFERENCED_PARAMETER(dwNumberOfBytesTransfered);
  UNREFERENCED_PARAMETER(lpOverlapped);
}

int _tmain()
{
  //Please note that all the following is just test code to exercise the CSerialPort API. It assumes
  //that a COM1 is available on the local computer.
  std::vector<BYTE> buf;
  buf.resize(10000);
  for (auto& b : buf)
    b = '%';
  HANDLE hEvent = nullptr;
  constexpr const int nPortToUse = 1;

  try
  {
    COMMCONFIG config;
    CSerialPort::GetDefaultConfig(nPortToUse, config);

    CSerialPort port;
    port.Open(nPortToUse, 1200, CSerialPort::Parity::NoParity, 8, CSerialPort::StopBits::OneStopBit, CSerialPort::FlowControl::XonXoffFlowControl);

    HANDLE hPort = port.Detach();
    port.Attach(hPort);

    DWORD dwModemStatus;
    port.GetModemStatus(dwModemStatus);

    DCB dcb;
    port.GetState(dcb);

    dcb.BaudRate = 9600;
    port.SetState(dcb);

    DWORD dwErrors = 0;
    port.ClearError(dwErrors);

    port.SetBreak();
    port.ClearBreak();

    COMSTAT stat{};
    port.GetStatus(stat);

    COMMTIMEOUTS timeouts{};
    port.GetTimeouts(timeouts);

    port.Setup(10000, 10000);

    port.GetConfig(config);

    config.dcb.BaudRate = 9600;
    port.SetConfig(config);

    port.Set0WriteTimeout();
    port.Set0ReadTimeout();

    const char* pszBuf = "This should appear on the serial port";
#pragma warning(suppress: 26472)
    port.Write(pszBuf, static_cast<DWORD>(strlen(pszBuf)));

    DWORD dwMask = 0;
    port.GetMask(dwMask);

    port.SetMask(EV_TXEMPTY); 

    //port.WaitEvent(dwMask);

    port.TerminateOutstandingWrites();

    port.TransmitChar('p');

    port.Set0Timeout();

    std::vector<char> rxBuf;
    rxBuf.resize(10);
#pragma warning(suppress: 26472)
    const DWORD dwRead = port.Read(rxBuf.data(), static_cast<DWORD>(rxBuf.size()));
    UNREFERENCED_PARAMETER(dwRead);

    port.TerminateOutstandingReads();

    port.ClearDTR();

    port.ClearRTS();

    port.SetDTR();

    port.SetRTS();

    //port.SetXOFF();

    //port.SetXON();

    COMMPROP properties{};
    port.GetProperties(properties);

    port.ClearWriteBuffer();

    port.ClearReadBuffer();

    port.Flush();

    port.CancelIo();

    port.Close();


    //Try out the overlapped functions
    CSerialPort port2;
    port2.Open(nPortToUse, 9600, CSerialPort::Parity::NoParity, 8, CSerialPort::StopBits::OneStopBit, CSerialPort::FlowControl::XonXoffFlowControl, TRUE);

    OVERLAPPED overlapped{};
    hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
#pragma warning(suppress: 26477)
    ATLASSERT(hEvent != nullptr);
    overlapped.hEvent = hEvent;

    try
    {
#pragma warning(suppress: 26472 26486)
      port2.Write(buf.data(), static_cast<DWORD>(buf.size()), overlapped);
    }
#pragma warning(suppress: 26496)
    catch(CSerialException& e)
    {
      if (e.m_dwError == ERROR_IO_PENDING)
      {
        DWORD dwBytesTransferred = 0;
        port2.GetOverlappedResult(overlapped, dwBytesTransferred, TRUE);
      }
      else
      {
        CSerialPort::ThrowSerialException(e.m_dwError);
      }
    }
    try
    {
      port2.Read(buf.data(), 10, overlapped);
    }
#pragma warning(suppress: 26496)
    catch(CSerialException& e)
    {
      if (e.m_dwError == ERROR_IO_PENDING)
      {
        DWORD dwBytesTransferred = 0;
        port2.GetOverlappedResult(overlapped, dwBytesTransferred, TRUE);
      }
      else
      {
        CSerialPort::ThrowSerialException(e.m_dwError);
      }
    }

    port2.SetMask(EV_TXEMPTY);

#pragma warning(suppress: 26472)
    port2.WriteEx(pszBuf, static_cast<DWORD>(strlen(pszBuf)), &overlapped, CompletionRoutine);
    SleepEx(INFINITE, TRUE);
    port2.ReadEx(buf.data(), 10, &overlapped, CompletionRoutine);
    SleepEx(INFINITE, TRUE);
  }
#pragma warning(suppress: 26496)
  catch (CSerialException& e)
  {
    ATLTRACE(_T("Unexpected CSerialPort exception, Error:%u\n"), e.m_dwError);
    UNREFERENCED_PARAMETER(e);
  }

  //Clean up the resources we have been using
  if (hEvent != nullptr)
  {
    CloseHandle(hEvent);
    hEvent = nullptr;
  }

  return 0;
}
