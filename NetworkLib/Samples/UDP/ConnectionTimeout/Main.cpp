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
						std::cout << "[Client 1]Unexpected connection from " << message->emitter().toString() << " (should be from " << client2.toString() << ")" << std::endl;
						continue;
					}
					else
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "[Client 1]Connection " << message->as<Bousk::Network::Messages::Connection>()->result << " from " << message->emitter().toString() << std::endl;
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
		std::chrono::milliseconds timeoutStart = std::chrono::milliseconds(0);
		constexpr auto Timeout = std::chrono::seconds(30); // Much higher than lib one
		for (bool exit = false; !exit && (timeoutStart == std::chrono::milliseconds(0) || timeoutStart + Timeout > Bousk::Utils::Now());)
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
						std::cout << "[Client 2]Unexpected connection received from " << message->emitter().toString() << " (should be from " << client1.toString() << ")" << std::endl;
						client.disconnect(message->emitter());
						continue;
					}
					else
					{
						std::scoped_lock lock(coutMutex);
						std::cout << "[Client 2]Receiving incoming connection from client 1 [" << message->emitter().toString() << "] and NOT accepting it..." << std::endl;
						timeoutStart = Bousk::Utils::Now();
					}
				}
				else if (message->is<Bousk::Network::Messages::Connection>())
				{
					std::scoped_lock lock(coutMutex);
					std::cout << "Unexpected connection from " << message->emitter().toString() << std::endl;
				}
				else if (message->is<Bousk::Network::Messages::Disconnection>())
				{
					std::scoped_lock lock(coutMutex);
					assert(message->emitter() == client1);
					std::cout << "Disconnection from " << message->emitter().toString() << "...[" << message->as<Bousk::Network::Messages::Disconnection>()->reason << "]" << std::endl;
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