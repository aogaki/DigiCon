// Data class, no getter and setter, only public members
#ifndef TEventData_HPP
#define TEventData_HPP 1

#include <cstdint>
#include <vector>

class TEventData
{
 public:
  TEventData() {};
  TEventData(uint32_t traceSize)
  {
    analogProbe1.resize(traceSize);
    analogProbe2.resize(traceSize);
    digitalProbe1.resize(traceSize);
    digitalProbe2.resize(traceSize);
  };
  TEventData(const TEventData &eventData)
  {
    module = eventData.module;
    channel = eventData.channel;
    timeStamp = eventData.timeStamp;
    timeStampNs = eventData.timeStampNs;
    energy = eventData.energy;
    energyShort = eventData.energyShort;
    flags = eventData.flags;
    analogProbe1 = eventData.analogProbe1;
    analogProbe1Type = eventData.analogProbe1Type;
    analogProbe2 = eventData.analogProbe2;
    analogProbe2Type = eventData.analogProbe2Type;
    digitalProbe1 = eventData.digitalProbe1;
    digitalProbe1Type = eventData.digitalProbe1Type;
    digitalProbe2 = eventData.digitalProbe2;
    digitalProbe2Type = eventData.digitalProbe2Type;
    waveformSize = eventData.waveformSize;
    eventSize = eventData.eventSize;
  };
  ~TEventData() {};

  uint8_t module;
  uint8_t channel;
  uint64_t timeStamp;
  double timeStampNs;
  uint16_t energy;
  int16_t energyShort;
  uint32_t flags;
  std::vector<int16_t> analogProbe1;
  int32_t analogProbe1Type;
  std::vector<int16_t> analogProbe2;
  int32_t analogProbe2Type;
  std::vector<uint8_t> digitalProbe1;
  int32_t digitalProbe1Type;
  std::vector<uint8_t> digitalProbe2;
  int32_t digitalProbe2Type;
  std::size_t waveformSize;
  uint32_t eventSize;
};
typedef std::vector<std::shared_ptr<TEventData>> DAQData_t;

class TSmallEventData
{
 public:
  TSmallEventData() {};
  TSmallEventData(const TSmallEventData &eventData)
  {
    module = eventData.module;
    channel = eventData.channel;
    timeStampNs = eventData.timeStampNs;
    energy = eventData.energy;
    energyShort = eventData.energyShort;
  };
  ~TSmallEventData() {};

  uint8_t module;
  uint8_t channel;
  double timeStampNs;
  uint16_t energy;
  int16_t energyShort;
};

#endif  // TEventData_HPP