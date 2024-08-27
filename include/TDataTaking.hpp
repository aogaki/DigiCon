#ifndef TDataTaking_HPP
#define TDataTaking_HPP 1
// Handle digitizers and readout data

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "TDigitizer.hpp"
#include "TEventData.hpp"

class TDataTaking
{
 public:
  TDataTaking();
  ~TDataTaking();

  void ForceTrace() { fForceTrace = true; };

  void LoadConfigFileList(const std::string &listName);

  void OpenDigitizers();
  void CloseDigitizers();

  void ConfigDigitizers();

  void StartAcquisition();
  void StopAcquisition();

  DAQData_t GetData();

  std::vector<uint32_t> GetNumberOfCh();
  std::vector<uint32_t> GetDeltaT();

 private:
  std::vector<std::string> fConfigFileList;
  std::vector<std::unique_ptr<TDigitizer>> fDigitizers;
  void LoadConfigFiles();

  void ResetEventsVec();
  DAQData_t fEventsVec;
  std::mutex fEventsVecMutex;

  bool fRunning = false;
  uint32_t fSleepTime = 1;  // in ms
  std::vector<std::thread> fAcquisitionThreads;
  void FetchingData();

  bool fForceTrace = false;
};

#endif  // TDataTaking_HPP