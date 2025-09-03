#ifndef SINUCA3_GSHARE_PREDICTOR_HPP_
#define SINUCA3_GSHARE_PREDICTOR_HPP_

#include <sinuca3.hpp>
#include <utils/bimodal_counter.hpp>
#include <utils/circular_buffer.hpp>

#include "engine/component.hpp"
#include "engine/default_packets.hpp"

class GsharePredictor : public Component<PredictorPacket> {
  private:
    BimodalCounter* entries;
    CircularBuffer indexQueue;
    unsigned long globalBranchHistReg;
    unsigned long numberOfEntries;
    unsigned long numberOfPredictions;
    unsigned long numberOfWrongpredictions;
    unsigned long currentIndex;
    unsigned int indexQueueSize;
    unsigned int indexBitsSize;
    bool directionPredicted;
    bool directionTaken;

    Component<PredictorPacket>* sendTo;
    int sendToId;

    int Allocate();
    void Deallocate();
    int RoundNumberOfEntries(unsigned long requestedSize);
    void PreparePacket(PredictorPacket* pkt);
    void ReadPacket(PredictorPacket* pkt);
    int EnqueueIndex();
    int DequeueIndex();
    void CalculateIndex(unsigned long addr);
    void UpdateEntry();
    void UpdateGlobBranchHistReg();
    void QueryEntry();

  public:
    GsharePredictor();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void PrintStatistics();
    virtual int FinishSetup();
    virtual void Clock();
    virtual ~GsharePredictor();
};

#endif  // SINUCA3_GSHARE_PREDICTOR