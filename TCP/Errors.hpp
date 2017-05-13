#ifndef BOUSK_DVP_COURS_ERRORS_HPP
#define BOUSK_DVP_COURS_ERRORS_HPP

#pragma once

#ifdef _WIN32
	#include <WinSock2.h>
#else
	#include <cerrno>
	#define SOCKET int
	#define INVALID_SOCKET ((int)-1)
	#define SOCKET_ERROR (int(-1))
#endif

namespace Sockets
{
	int GetError();
	enum class Errors : int {
#ifdef _WIN32
		WOULDBLOCK = WSAEWOULDBLOCK,
		INPROGRESS = WSAEINPROGRESS,
#else
		WOULDBLOCK = EWOULDBLOCK,
		INPROGRESS = EINPROGRESS,
#endif
	};
}

#endif // BOUSK_DVP_COURS_ERRORS_HPP