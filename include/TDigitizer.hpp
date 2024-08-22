#ifndef TDigitizer_HPP
#define TDigitizer_HPP 1

#include <CAEN_FELib.h>

#include <TEventData.hpp>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

class TDigitizer
{
 public:
  TDigitizer();
  virtual ~TDigitizer();

  void TestScope();

  void LoadParameters(const std::string &filename);
  void OpenDigitizer();
  void CloseDigitizer();
  void ConfigDigitizer();

  void StartAcquisition();
  void StopAcquisition();

  void SetDataFormat();

  std::unique_ptr<std::vector<std::unique_ptr<TEventData>>> GetEvents();

 private:
  uint8_t fModNo = 0;
  uint64_t fHandle;
  uint64_t fReadDataHandle;
  nlohmann::json fParameters;
  std::string fFW;
  int32_t fTimeOut = 100;

  std::unique_ptr<std::vector<std::unique_ptr<TEventData>>> fEventsVec;
  void MakeNewEventsVec();
  std::mutex fEventsDataMutex;
  std::thread fAcquisitionThread;
  bool fRunning = false;

  void FetchEventsPSD();
  void FetchEventsPHA();
  void FetchEventsScope();

  void CheckDigitizer() const;
  void PrintDigitizerInfo() const;

  void CheckError(int err) const;

  bool SendCommand(std::string path) const;

  bool GetParameter(std::string path, std::string &values) const;
  bool SetParameter(std::string path, std::string value) const;
};

#endif  // TDigitizer_HPP