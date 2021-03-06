#include "Sockets.hpp"
#include "UDP/UDPClient.hpp"
#include "UDP/Protocols/ReliableOrdered.hpp"
#include "Serialization/Deserializer.hpp"
#include "Serialization/Serializer.hpp"
#include "Messages.hpp"
#include "Errors.hpp"

#include <iostream>
#include <mutex>
#include <string>
#include <thread>

int main()
{
	if (!Bousk::Network::Start())
	{
		std::cout << "Network lib initialisation error : " << Bousk::Network::Errors::Get();
		return -1;
	}

	const Bousk::Network::Address client1 = Bousk::Network::Address::Loopback(Bousk::Network::Address::Type::IPv4, 8888);
	const Bousk::Network::Address client2 = Bousk::Network::Address::Loopback(Bousk::Network::Address::Type::IPv4, 9999);

	std::mutex coutMutex;
	// Create a thread per client
	std::thread t1([&]()
	{
		Bousk::Network::UDP::Client client;
		client.registerChannel<Bousk::Network::UDP::Protocols::ReliableOrdered>();
		if (!client.init(client1.port()))
		{
			std::scoped_lock lock(coutMutex);
			std::cout << "Client 1 initialisation error : " << Bousk::Network::Errors::Get();
			return;
		}
		{
			std::scoped_lock lock(coutMutex);
			std::cout << "Client 1 initialized on port " << client1.port() << std::endl;
		}
		// Connect client 1 to client 2
		client.connect(client2);
		{
			std::scoped_lock lock(coutMutex);
			std::cout << "Client 1 connecting to " << client2.toString() << "..." << std::endl;
		}
		std::vector<std::string> receivedMessages;
		for (bool exit = false; !exit;)
		{
			client.receive();
			auto messages = client.poll();
			for (auto&& message : messages)
			{
				if (message->is<Bousk::Network::Messages::Connection>())
				{
					if (message->emitter() != client2)
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "Unexpected connection from " << message->emitter().toString() << " (should be from " << client2.toString() << ")" << std::endl;
						continue;
					}
					else
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "Client 2 [" << client2.toString() << "] connected to client 1" << std::endl;
						std::cout << "Shutting down client 1..." << std::endl;
						exit = true;
					}
				}
			}
			client.processSend();
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		client.release();
	});
	std::thread t2([&]()
	{
		Bousk::Network::UDP::Client client;
		client.registerChannel<Bousk::Network::UDP::Protocols::ReliableOrdered>();
		if (!client.init(client2.port()))
		{
			std::scoped_lock lock(coutMutex);
			std::cout << "Client 2 initialisation error : " << Bousk::Network::Errors::Get();
			return;
		}
		{
			std::scoped_lock lock(coutMutex);
			std::cout << "Client 2 initialized on port " << client2.port() << std::endl;
		}
		for (bool exit = false; !exit;)
		{
			client.receive();
			auto messages = client.poll();
			for (auto&& message : messages)
			{
				if (message->is<Bousk::Network::Messages::IncomingConnection>())
				{
					if (message->emitter() != client1)
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "Unexpected connection received from " << message->emitter().toString() << " (should be from " << client1.toString() << ")" << std::endl;
						client.disconnect(message->emitter());
						continue;
					}
					else
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "Client 2 receiving incoming connection from [" << message->emitter().toString() << "] (client 1)... and accepting it" << std::endl;
					}
					client.connect(message->emitter());
				}
				else if (message->is<Bousk::Network::Messages::Connection>())
				{
					if (message->emitter() != client1)
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "Unexpected connection from " << message->emitter().toString() << " (should be from " << client1.toString() << ")" << std::endl;
						continue;
					}
					else
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "Client 1 [" << client1.toString() << "] connected to client 2" << std::endl;
					}
				}
				else if (message->is<Bousk::Network::Messages::Disconnection>())
				{
					std::scoped_lock lock(coutMutex);
					assert(message->emitter() == client1);
					std::cout << "Disconnection from client 1...[" << message->as<Bousk::Network::Messages::Disconnection>()->reason << "]" << std::endl;
					exit = true;
				}
			}
			client.processSend();
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		std::cout << "Normal termination." << std::endl;
		client.release();
	});

	t1.join();
	t2.join();

	Bousk::Network::Release();
	return 0;
}