#ifndef __CPU_AME_REQ_STATE_HH__
#define __CPU_AME_REQ_STATE_HH__

#include <cassert>
#include <cstdint>
#include <functional>

#include "mem/packet.hh"
#include "mem/request.hh"

namespace  gem5 {


class AME_ReqState
{
  public:
    AME_ReqState(uint64_t req_id) :
        reqId(req_id), matched(false), pkt(nullptr)
    {}
    virtual ~AME_ReqState() {
        if (pkt != nullptr) {
            assert(pkt->req != nullptr);
            //delete pkt->req;
            delete pkt;
        }
    }

    //read/write have different callback type-definitions. so abstract it here
    virtual void executeCallback() = 0;

    uint64_t getReqId() { return reqId; }
    bool isMatched() { return matched; }
    void setPacket(PacketPtr _pkt) {
        assert(!matched);
        matched = true;
        pkt = _pkt;
    }

  protected:
    uint64_t reqId;
    bool matched;
    PacketPtr pkt;
};


class AME_R_ReqState : public AME_ReqState
{
  public:
    AME_R_ReqState(uint64_t req_id,
        std::function<void(uint8_t*data, uint8_t size)> callback) :
        AME_ReqState(req_id), callback(callback) {}
    ~AME_R_ReqState() {}

    void executeCallback() override {
        assert(matched);
        callback(pkt->getPtr<uint8_t>(), pkt->getSize());
    }

  private:
    std::function<void(uint8_t*data, uint8_t size)> callback;
};

class AME_W_ReqState : public AME_ReqState
{
  public:
    AME_W_ReqState(uint64_t req_id, std::function<void(void)> callback) :
        AME_ReqState(req_id), callback(callback) {}
    ~AME_W_ReqState() {}

    void executeCallback() override {
        assert(matched);
        callback();
    }

  private:
    std::function<void(void)> callback;
};



}

#endif 