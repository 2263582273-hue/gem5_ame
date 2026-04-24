#ifndef __CPU_AME_PACKET_HH__
#define __CPU_AME_PACKET_HH__

#include <cstdint>

#include "mem/packet.hh"
#include "mem/request.hh"

class AMEPacket;

typedef AMEPacket * AMEPacketPtr;

class AMEPacket : public gem5::Packet
{
  public:

    AMEPacket(gem5::RequestPtr req,gem5::MemCmd cmd,uint64_t req_id,uint8_t channel) :
        Packet(req, cmd), reqId(req_id), channel(channel) {}
    AMEPacket(gem5::RequestPtr req, gem5::MemCmd cmd, uint64_t req_id) :
        Packet(req, cmd), reqId(req_id), channel(0) {}
    ~AMEPacket() {}

    const uint64_t reqId;
    const uint8_t channel;
};

#endif // __CPU_VECTOR_PACKET_HH__
