/*
 * Multiplatform Async Network Library
 * Copyright (c) 2007 Burlex
 *
 * Socket implementable class.
 * Modded by 2009 Cebernic
 */

#include "Network.h"

initialiseSingleton(SocketGarbageCollector);

Socket::Socket(SOCKET fd, uint32 sendbuffersize, uint32 recvbuffersize) : m_fd(fd), m_connected(false),	m_deleted(false)
{
	// Allocate Buffers
	readBuffer.Allocate(recvbuffersize);
	writeBuffer.Allocate(sendbuffersize);

	_recvbuffsize = recvbuffersize;
	_sendbuffsize = sendbuffersize;

	// IOCP Member Variables
#ifdef CONFIG_USE_IOCP
	m_writeLock = 0;
	m_completionPort = 0;
#else
	m_writeLock = 0;
#endif
	guid = 0;

	total_send_bytes=0;
	total_read_bytes=0;

	snapshot_send_bytes=0;
	snapshot_read_bytes=0;

	// Check for needed fd allocation.
	if(m_fd == 0)
		m_fd = SocketOps::CreateTCPFileDescriptor();
}

Socket::~Socket()
{
	OnDisconnect();
	
	if ( m_connected || !m_deleted ){
		sSocketMgr.RemoveSocket(this);
	}

	m_deleted = true;
	m_connected = false;

	if(m_fd != 0) SocketOps::CloseSocket(m_fd);
}

bool Socket::Connect(const char * Address, uint32 Port)
{
	if ( m_deleted ) return false;
	struct hostent * ci = gethostbyname(Address);
	if(ci == 0)
		return false;

	m_client.sin_family = AF_INET;//ci->h_addrtype;
	m_client.sin_port = ntohs((u_short)Port);

	memset(&m_client.sin_addr,0,sizeof(in_addr));
	memcpy(&m_client.sin_addr, ci->h_addr_list[0], sizeof(in_addr));

	SocketOps::Blocking(m_fd);
	SocketOps::SetTimeout(m_fd,2);
	if(connect(m_fd, (const sockaddr*)&m_client, sizeof(m_client)) == -1)
	{
		SocketOps::CloseSocket(m_fd);
		return false;
	}

	// at this point the connection was established
#ifdef CONFIG_USE_IOCP
	m_completionPort = sSocketMgr.GetCompletionPort();
#endif
	_OnConnect();
	return true;
}

void Socket::Accept(sockaddr_in * address)
{
	if ( m_deleted ) return;
	memcpy(&m_client, address, sizeof(*address));
	_OnConnect();
}

void Socket::_OnConnect()
{
	SocketOps::Nonblocking(m_fd);
	SocketOps::DisableBuffering(m_fd);

	// for fast garbage collector.
	setGUID(GenerateGUID());
	setLastheartbeat(); // first set

#ifdef CONFIG_USE_IOCP
	m_deleted = false;
	sSocketGarbageCollector.QueueSocket(this); 
	SocketOps::SetRecvBufferSize(m_fd, _recvbuffsize );
	SocketOps::SetSendBufferSize(m_fd, _sendbuffsize );
	BOOL bDontLinger = FALSE;
	setsockopt(m_fd,SOL_SOCKET,SO_DONTLINGER,(const char*)&bDontLinger,sizeof(BOOL)); // notimewait
#else
	sSocketMgr.AddSocket(this);
	m_connected = true;
	m_deleted = false;
#endif

	// IOCP stuff
#ifdef CONFIG_USE_IOCP
	AssignToCompletionPort();
	SetupReadEvent(0); // Recv now
#endif

	// Call virtual onconnect
	OnConnect();
}

bool Socket::Send(const uint8 * Bytes, uint32 Size)
{
	bool rv;

	// This is really just a wrapper for all the burst stuff.
	BurstBegin();
	rv = BurstSend(Bytes, Size);
	if(rv)
		BurstPush();
	BurstEnd();

	return rv;
}

bool Socket::BurstSend(const uint8 * Bytes, uint32 Size)
{
	return writeBuffer.Write(Bytes, Size);
}

string Socket::GetRemoteIP()
{
	char* ip = (char*)inet_ntoa( m_client.sin_addr );
	if( ip != NULL )
		return string( ip );
	else
		return string( "noip" );
}

void Socket::Disconnect(bool no_garbageprocess)
{
	// remove from mgr
	if ( m_connected ) {
		sSocketMgr.RemoveSocket(this);
		//OnDisconnect();
	}

	m_connected = false;

	if(!m_deleted) {
		if ( !no_garbageprocess ){
			sSocketGarbageCollector.QueueSocket(this); // send this to garbage collector.
		}
		m_deleted = true;
	}

	SocketOps::Blocking(m_fd);
	SocketOps::CloseSocket(m_fd);

}

void Socket::Delete(bool force)
{
	Disconnect(force); // force true ,we will not queue the socket.
	if ( force ) delete this; // delete immediately!
}

bool SocketGarbageCollectorThread::run()
{
	bool run();
	{
		uint32 loop=0;
		while( open && sSocketGarbageCollector.running )
		{
			loop++;

			sSocketGarbageCollector.Update(); // garbage system

#ifdef CONFIG_USE_IOCP

			if ( !(loop%60) ) // 60 second for checkdeadsocket *internal*
				sSocketMgr.CheckDeadSocket_Internal();

			if ( !(loop%10) )  // 10 second for snapshot
				sSocketMgr.MakeSnapShot();

#endif

			if (loop>101)loop=0;
			Sleep(1000);
		}
		sSocketGarbageCollector.sgthread=NULL;
		return true; // threadpool delete us.
	}
}
