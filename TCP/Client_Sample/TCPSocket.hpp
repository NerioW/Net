#ifndef BOUSK_DVP_SOCKET_COURS_TCPSOCKET_HPP
#define BOUSK_DVP_SOCKET_COURS_TCPSOCKET_HPP

#pragma once

#include "Sockets.hpp"

#include <string>

class TCPSocket
{
	public:
		TCPSocket();
		~TCPSocket();

		bool Connect(const std::string& ipaddress, unsigned short port);

	private:
		SOCKET mSocket;
};


#endif // BOUSK_DVP_SOCKET_COURS_TCPSOCKET_HPP