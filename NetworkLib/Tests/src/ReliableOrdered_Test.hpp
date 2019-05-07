#pragma once

#include "Tester.hpp"
#include "UDP/Protocoles/ReliableOrdered.hpp"

#include <limits>
#include <chrono>
#include <random>

class ReliableOrdered_Multiplexer_Test
{
public:
	static void Test();
};

void ReliableOrdered_Multiplexer_Test::Test()
{
	Bousk::Network::UDP::Protocoles::ReliableOrdered::Multiplexer mux;
	CHECK(mux.mQueue.size() == 0);
	CHECK(mux.mNextId == 0);
	CHECK(mux.mFirstAllowedPacket == 0);
	Bousk::Network::UDP::Datagram::ID datagramId = 0;
	{
		const std::array<uint8_t, 5> arr{ 'T', 'o', 't', 'o', '\0' };
		std::vector<uint8_t> data(arr.cbegin(), arr.cend());
		mux.queue(std::move(data));
		CHECK(mux.mQueue.size() == 1);
		CHECK(mux.mNextId == 1);
		CHECK(mux.mFirstAllowedPacket == 0);

		const Bousk::Network::UDP::Datagram::ID sentDatagramId = datagramId;
		std::array<uint8_t, Bousk::Network::UDP::Packet::PacketMaxSize> buffer;
		const size_t serializedData = mux.serialize(buffer.data(), buffer.size(), datagramId++);
		CHECK(serializedData == Bousk::Network::UDP::Packet::HeaderSize + arr.size());
		const Bousk::Network::UDP::Packet* packet = reinterpret_cast<const Bousk::Network::UDP::Packet*>(buffer.data());
		CHECK(packet->datasize() == arr.size());
		CHECK(memcmp(packet->data(), arr.data(), packet->datasize()) == 0);
		//!<The message is still in the queue until acked
		CHECK(mux.mQueue.size() == 1);
		//!<Ack this datagram
		mux.onDatagramAcked(sentDatagramId);
		CHECK(mux.mQueue.size() == 0);
		CHECK(mux.mFirstAllowedPacket == 1);
	}
	{
		//!< Send fragmented message : 3 fragments
		std::vector<uint8_t> data(Bousk::Network::UDP::Packet::DataMaxSize * 3, 0);
		const unsigned int seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
		std::default_random_engine generator(seed);
		std::uniform_int_distribution<unsigned int> distribution(0, 100);
		for (uint8_t& d : data)
			d = distribution(generator);

		const auto datacopy = data;
		mux.queue(std::move(data));
		CHECK(mux.mQueue.size() == 3);
		CHECK(mux.mNextId == 4);
		size_t totalDataSize = 0;
		const Bousk::Network::UDP::Datagram::ID firstSentDatagramId = datagramId;
		const size_t nbPackets = mux.mQueue.size();
		for (;;)
		{
			std::array<uint8_t, Bousk::Network::UDP::Packet::PacketMaxSize> buffer;
			const size_t serializedData = mux.serialize(buffer.data(), buffer.size(), datagramId++);
			if (serializedData == 0)
				break;
			const Bousk::Network::UDP::Packet* packet = reinterpret_cast<const Bousk::Network::UDP::Packet*>(buffer.data());
			CHECK(memcmp(packet->data(), datacopy.data() + totalDataSize, packet->datasize()) == 0);
			totalDataSize += serializedData - Bousk::Network::UDP::Packet::HeaderSize;
		}
		CHECK(totalDataSize == datacopy.size());
		CHECK(mux.mQueue.size() == 3);
		//!<Ack each of them
		size_t acked = 0;
		for (auto i = 0; i < nbPackets; ++i)
		{
			const Bousk::Network::UDP::Datagram::ID dgramId = firstSentDatagramId + i;
			mux.onDatagramAcked(dgramId);
			++acked;
			CHECK(mux.mQueue.size() == 3 - acked);
		}
		CHECK(mux.mFirstAllowedPacket == 4);
	}
	{
		//!< Send a message which will get lost
		const std::array<uint8_t, 5> arr{ 'T', 'i', 't', 'i', '\0' };
		std::vector<uint8_t> data(arr.cbegin(), arr.cend());
		mux.queue(std::move(data));
		CHECK(mux.mQueue.size() == 1);
		const Bousk::Network::UDP::Datagram::ID lostDatagramId = datagramId;

		{
			std::array<uint8_t, Bousk::Network::UDP::Packet::PacketMaxSize> buffer;
			const size_t serializedData = mux.serialize(buffer.data(), buffer.size(), datagramId++);
			CHECK(serializedData == Bousk::Network::UDP::Packet::HeaderSize + arr.size());
			const Bousk::Network::UDP::Packet* packet = reinterpret_cast<const Bousk::Network::UDP::Packet*>(buffer.data());
			CHECK(packet->datasize() == arr.size());
			CHECK(memcmp(packet->data(), arr.data(), packet->datasize()) == 0);
		}
		CHECK(mux.mQueue.size() == 1);
		CHECK(mux.mFirstAllowedPacket == 4);
		
		//!<The packet hasn't been lost yet, we should have nothing to serialize
		{
			std::array<uint8_t, Bousk::Network::UDP::Packet::PacketMaxSize> buffer;
			const size_t serializedData = mux.serialize(buffer.data(), buffer.size(), datagramId++);
			CHECK(serializedData == 0);
		}
		CHECK(mux.mQueue.size() == 1);
		mux.onDatagramLost(lostDatagramId);
		CHECK(mux.mFirstAllowedPacket == 4);

		const Bousk::Network::UDP::Datagram::ID datagramIdToAck = datagramId;
		{
			//!< Resend
			std::array<uint8_t, Bousk::Network::UDP::Packet::PacketMaxSize> buffer;
			const size_t serializedData = mux.serialize(buffer.data(), buffer.size(), datagramId++);
			CHECK(serializedData == Bousk::Network::UDP::Packet::HeaderSize + arr.size());
			const Bousk::Network::UDP::Packet* packet = reinterpret_cast<const Bousk::Network::UDP::Packet*>(buffer.data());
			CHECK(packet->datasize() == arr.size());
			CHECK(memcmp(packet->data(), arr.data(), packet->datasize()) == 0);
		}
		CHECK(mux.mQueue.size() == 1);
		mux.onDatagramAcked(datagramIdToAck);
		//!< Now it's acked
		CHECK(mux.mQueue.size() == 0);
		CHECK(mux.mFirstAllowedPacket == 5);
	}
	{
		//!< 3 messages, lose the middle one
		std::vector<uint8_t> data(Bousk::Network::UDP::Packet::DataMaxSize * 3, 0);
		const unsigned int seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
		std::default_random_engine generator(seed);
		std::uniform_int_distribution<unsigned int> distribution(0, 100);
		for (uint8_t& d : data)
			d = distribution(generator);

		const auto datacopy = data;
		mux.queue(std::move(data));
		CHECK(mux.mQueue.size() == 3);
		CHECK(mux.mFirstAllowedPacket == 5);

		const auto datagram1 = datagramId;
		std::array<uint8_t, Bousk::Network::UDP::Packet::PacketMaxSize> buffer;
		mux.serialize(buffer.data(), buffer.size(), datagramId++);
		const auto datagram2 = datagramId;
		mux.serialize(buffer.data(), buffer.size(), datagramId++);
		const auto datagram3 = datagramId;
		mux.serialize(buffer.data(), buffer.size(), datagramId++);
		CHECK(mux.mFirstAllowedPacket == 5);
		mux.onDatagramAcked(datagram3);
		CHECK(mux.mFirstAllowedPacket == 5);
		mux.onDatagramAcked(datagram2);
		CHECK(mux.mFirstAllowedPacket == 5);
		mux.onDatagramAcked(datagram1);
		CHECK(mux.mFirstAllowedPacket == 8);
	}
}

class ReliableOrdered_Demultiplexer_Test
{
public:
	static void Test();
};

void ReliableOrdered_Demultiplexer_Test::Test()
{
	//!< Use the multiplexer to easily queue and split data
	//!< It's been tested before so it's reliable
	Bousk::Network::UDP::Protocoles::ReliableOrdered::Multiplexer mux;
	Bousk::Network::UDP::Protocoles::ReliableOrdered::Demultiplexer demux;
	CHECK(demux.mLastProcessed == std::numeric_limits<Bousk::Network::UDP::Packet::Id>::max());
	Bousk::Network::UDP::Datagram::ID datagramId = 0;
	{
		const std::array<uint8_t, 5> arr0{ 'T', 'o', 't', 'o', '\0' };
		std::vector<uint8_t> data0(arr0.cbegin(), arr0.cend());
		//!< Receive packet 0 & 1
		{
			mux.queue(std::move(data0));
			Bousk::Network::UDP::Packet packet;
			mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			CHECK(packet.id() == 0);
			CHECK(packet.type() == Bousk::Network::UDP::Packet::Type::Packet);
			CHECK(packet.datasize() == static_cast<uint16_t>(arr0.size()));
			CHECK(memcmp(packet.data(), arr0.data(), arr0.size()) == 0);
			demux.onDataReceived(packet.buffer(), packet.size());
		}
		const std::array<uint8_t, 5> arr1{ 'T', 'a', 't', 'a', '\0' };
		std::vector<uint8_t> data1(arr1.cbegin(), arr1.cend());
		{
			mux.queue(std::move(data1));
			Bousk::Network::UDP::Packet packet;
			mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			CHECK(packet.id() == 1);
			CHECK(packet.type() == Bousk::Network::UDP::Packet::Type::Packet);
			CHECK(packet.datasize() == static_cast<uint16_t>(arr1.size()));
			CHECK(memcmp(packet.data(), arr1.data(), arr1.size()) == 0);
			demux.onDataReceived(packet.buffer(), packet.size());
		}
		const std::vector<std::vector<uint8_t>> packets = demux.process();
		CHECK(packets.size() == 2);
		CHECK(demux.mLastProcessed == 1);
		CHECK(packets[0].size() == data0.size());
		CHECK(packets[0] == data0);
		CHECK(packets[1].size() == data1.size());
		CHECK(packets[1] == data1);
	}
	{
		//!< Receive fragmented message : 3 fragments
		std::vector<uint8_t> data(Bousk::Network::UDP::Packet::DataMaxSize * 3, 0);
		const unsigned int seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
		std::default_random_engine generator(seed);
		std::uniform_int_distribution<unsigned int> distribution(0, 100);
		for (uint8_t& d : data)
			d = distribution(generator);

		const auto datacopy = data;
		mux.queue(std::move(data));
		{
			const auto sentDatagramdId = datagramId;
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			demux.onDataReceived(packet.buffer(), packet.size());
			CHECK(demux.process().empty());
			mux.onDatagramAcked(sentDatagramdId);
		}
		{
			const auto sentDatagramdId = datagramId;
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			demux.onDataReceived(packet.buffer(), packet.size());
			CHECK(demux.process().empty());
			mux.onDatagramAcked(sentDatagramdId);
		}
		{
			const auto sentDatagramdId = datagramId;
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			demux.onDataReceived(packet.buffer(), packet.size());
			mux.onDatagramAcked(sentDatagramdId);
		}
		{
			const auto sentDatagramdId = datagramId;
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			CHECK(serializedData == 0);
			mux.onDatagramAcked(sentDatagramdId);
		}
		const std::vector<std::vector<uint8_t>> packets = demux.process();
		CHECK(packets.size() == 1);
		CHECK(demux.mLastProcessed == 4);
		CHECK(packets[0].size() == datacopy.size());
		CHECK(packets[0] == datacopy);
	}
	{
		//!< Receive fragmented message : 3 fragments, second fragment lost
		std::vector<uint8_t> data(Bousk::Network::UDP::Packet::DataMaxSize * 3, 0);
		const unsigned int seed = static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
		std::default_random_engine generator(seed);
		std::uniform_int_distribution<unsigned int> distribution(0, 100);
		for (uint8_t& d : data)
			d = distribution(generator);

		const auto datacopy = data;
		mux.queue(std::move(data));
		{
			const auto sentDatagramdId = datagramId;
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			demux.onDataReceived(packet.buffer(), packet.size());
			CHECK(demux.process().empty());
			mux.onDatagramAcked(sentDatagramdId);
		}
		const auto lostDatagramdId = datagramId;
		{
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
		}
		{
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			demux.onDataReceived(packet.buffer(), packet.size());
		}
		{
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			CHECK(serializedData == 0);
		}
		{
			mux.onDatagramLost(lostDatagramdId);
			Bousk::Network::UDP::Packet packet;
			const size_t serializedData = mux.serialize(reinterpret_cast<uint8_t*>(&packet), Bousk::Network::UDP::Packet::PacketMaxSize, datagramId++);
			demux.onDataReceived(packet.buffer(), packet.size());
		}
		const std::vector<std::vector<uint8_t>> packets = demux.process();
		CHECK(packets.size() == 1);
		CHECK(demux.mLastProcessed == 7);
		CHECK(packets[0].size() == datacopy.size());
		CHECK(packets[0] == datacopy);
	}
}

class ReliableOrdered_Test
{
public:
	static void Test()
	{
		ReliableOrdered_Multiplexer_Test::Test();
		ReliableOrdered_Demultiplexer_Test::Test();
	}
};