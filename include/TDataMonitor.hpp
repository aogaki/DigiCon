#ifndef TDataMonitor_HPP
#define TDataMonitor_HPP 1

// Using THttpServer to monitor the data taking

#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>
#include <THttpServer.h>

#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "TEventData.hpp"

class TDataMonitor
{
 public:
  TDataMonitor();
  ~TDataMonitor();

  void LoadChannelConf(const std::vector<uint32_t> &nChs = {64, 64, 64, 64, 64,
                                                            64, 64, 64});
  void SetDeltaT(const std::vector<uint32_t> &deltaT) { fDeltaT = deltaT; }

  void StartMonitor();
  void StopMonitor();

  void SetData(std::shared_ptr<DAQData_t> data);

  void ClearHist();

 private:
  std::unique_ptr<THttpServer> fServer;
  std::vector<std::vector<std::unique_ptr<TGraph>>> fGraphAP1;
  std::vector<std::vector<std::unique_ptr<TGraph>>> fGraphAP2;
  std::vector<std::vector<std::unique_ptr<TGraph>>> fGraphDP1;
  std::vector<std::vector<std::unique_ptr<TGraph>>> fGraphDP2;
  std::vector<std::vector<std::unique_ptr<TH1D>>> fHist;
  std::vector<std::vector<std::unique_ptr<TCanvas>>> fCanvas;
  std::vector<uint32_t> fModAndCh;
  std::vector<uint32_t> fDeltaT;
  // Mutex is not big size. It is OK to use more than really needed
  static constexpr uint32_t fNChs = 64;
  static constexpr uint32_t fNMods = 16;
  std::mutex fAP1Mutex[fNMods][fNChs];
  std::mutex fAP2Mutex[fNMods][fNChs];
  std::mutex fDP1Mutex[fNMods][fNChs];
  std::mutex fDP2Mutex[fNMods][fNChs];
  void InitHist();
  void InitGraph();
  void InitCanvas();
  void RegisterHistCanvas();

  bool fMonitorRunning;
  std::deque<std::shared_ptr<DAQData_t>> fDataQueue;
  std::mutex fDataQueueMutex;
  std::mutex fThreadMutex;
  std::vector<std::thread> fThreadPool;
  void FillingThread(uint32_t iThread);

  void ROOTThread();
};

#endif  // TDataMonitor_HPP