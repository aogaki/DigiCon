#ifndef TDataRecorder_hpp
#define TDataRecorder_hpp 1

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "TEventData.hpp"

class TDataRecorder
{
 public:
  TDataRecorder();
  ~TDataRecorder();

  void StartRecording();
  void StopRecording();

  void SetData(std::unique_ptr<DAQData_t> data);
  void SetSizeLimit(const uint32_t &maxSize);
  void SetTimeLimit(const uint32_t &minutes);
  void SetFileName(const std::string &fileName);

 private:
  bool fRecording;

  uint32_t fFileSize = 100 * 1024 * 1024;  // 100 MB
  uint32_t fDataSize;
  std::chrono::minutes fMaxTime{30};
  std::chrono::system_clock::time_point fLastWrite;
  std::string fFileName = "tmp";
  uint32_t fFileVersion;
  std::mutex fFileMutex;

  std::deque<std::unique_ptr<DAQData_t>> fRawDataQue;
  std::mutex fRawDataQueMutex;

  std::vector<TSmallEventData *> fDataVec;
  std::mutex fDataVecMutex;

  std::vector<std::thread> fThreadPool;
  void WritingThread();
  void ConvertingThread();
  void WriteData(std::vector<TSmallEventData *> &data);

  void PostProcess();
};

#endif  // TDataRecorder_hpp