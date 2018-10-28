#pragma once

#include "UDP/Datagram.hpp"
#include <cstdint>
#include <array>

namespace Bousk
{
	namespace Network
	{
		namespace UDP
		{
			struct Packet
			{
				using Id = uint16_t;
				enum class Type : uint8_t
				{
					Packet,
					FirstFragment,
					Fragment,
					LastFragment,
				};
				struct Header
				{
					Id id;
					uint16_t size;
					Type type;
				};
				static constexpr size_t PacketMaxSize = Datagram::DataMaxSize;
				static constexpr size_t DataMaxSize = PacketMaxSize - sizeof(Header);

				Header header;
				std::array<unsigned char, DataMaxSize> data;
			};
		}
	}
}