#include "TDataRecorder.hpp"

#include <TFile.h>
#include <TROOT.h>
#include <TTree.h>

#include <algorithm>
#include <iostream>
#include <parallel/algorithm>

TDataRecorder::TDataRecorder() { fRecording = false; }

TDataRecorder::~TDataRecorder() { StopRecording(); }

void TDataRecorder::SetData(std::unique_ptr<DAQData_t> data)
{
  {
    std::lock_guard<std::mutex> lock(fRawDataQueMutex);
    fRawDataQue.push_back(std::move(data));
  }
}

void TDataRecorder::SetSizeLimit(const uint32_t &maxSize)
{
  fFileSize = maxSize;
}

void TDataRecorder::SetTimeLimit(const uint32_t &minutes)
{
  fMaxTime = std::chrono::minutes(minutes);
}

void TDataRecorder::SetFileName(const std::string &fileName)
{
  fFileName = fileName;
}

void TDataRecorder::ConvertingThread()
{
  std::unique_ptr<DAQData_t> localData = nullptr;

  constexpr auto modSize = sizeof(TSmallEventData::module);
  constexpr auto chSize = sizeof(TSmallEventData::channel);
  constexpr auto tsSize = sizeof(TSmallEventData::timeStampNs);
  constexpr auto enSize = sizeof(TSmallEventData::energy);
  constexpr auto enShortSize = sizeof(TSmallEventData::energyShort);
  constexpr auto oneHitSize = modSize + chSize + tsSize + enSize + enShortSize;
  constexpr auto wfSize = sizeof(TSmallEventData::waveform[0]);

  while (fRecording) {
    {
      std::lock_guard<std::mutex> lock(fRawDataQueMutex);
      if (!fRawDataQue.empty()) {
        localData = std::move(fRawDataQue.front());
        fRawDataQue.pop_front();
      }
    }

    if (localData) {
      std::vector<TSmallEventData *> localDataVec;
      uint32_t localDataSize = 0;

      for (const auto &event : *localData) {
        if (fRecording == false) break;
        TSmallEventData smallEvent;
        smallEvent.module = event->module;
        smallEvent.channel = event->channel;
        smallEvent.timeStampNs = event->timeStampNs;
        smallEvent.energy = event->energy;
        smallEvent.energyShort = event->energyShort;
        smallEvent.waveform.clear();
        if (event->waveformSize > 0)
          smallEvent.waveform.insert(smallEvent.waveform.end(),
                                     event->analogProbe1.begin(),
                                     event->analogProbe1.end());

        localDataVec.emplace_back(new TSmallEventData(smallEvent));
        localDataSize += oneHitSize + event->waveformSize * wfSize;
      }

      __gnu_parallel::sort(localDataVec.begin(), localDataVec.end(),
                           [](const auto &a, const auto &b) {
                             return a->timeStampNs < b->timeStampNs;
                           });

      {
        std::lock_guard<std::mutex> lock(fDataVecMutex);
        fDataVec.insert(fDataVec.end(), localDataVec.begin(),
                        localDataVec.end());
        fDataSize += localDataSize;
      }

      localData.reset();
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void TDataRecorder::WritingThread()
{
  ROOT::EnableThreadSafety();
  constexpr auto mergineSize = 1.1;

  while (fRecording) {
    bool timeCondition = false;
    bool sizeCondition = false;
    std::vector<TSmallEventData *> localDataVec;

    auto now = std::chrono::system_clock::now();
    {
      std::lock_guard<std::mutex> lock(fDataVecMutex);
      timeCondition = now - fLastWrite > fMaxTime;
      sizeCondition = fDataSize > fFileSize * mergineSize;
      if (timeCondition || sizeCondition) {
        localDataVec.insert(localDataVec.end(), fDataVec.begin(),
                            fDataVec.end());
        fDataVec.clear();
        fDataSize = 0;
      }
    }

    if (!localDataVec.empty()) {
      __gnu_parallel::sort(localDataVec.begin(), localDataVec.end(),
                           [](const auto &a, const auto &b) {
                             return a->timeStampNs < b->timeStampNs;
                           });

      if (sizeCondition) {
        auto th = uint32_t(localDataVec.size() / mergineSize);
        std::lock_guard<std::mutex> lock(fDataVecMutex);
        fDataVec.insert(fDataVec.end(), localDataVec.begin() + th,
                        localDataVec.end());
        localDataVec.resize(th);
      }

      auto fileName = fFileName;
      {
        std::lock_guard<std::mutex> lock(fFileMutex);
        fileName += std::string("_") + std::to_string(fFileVersion) + ".root";
        fFileVersion++;
        fLastWrite = now;
        std::cout << "Writing to " << fileName;
        if (timeCondition) std::cout << " due to time limit";
        if (sizeCondition) std::cout << " due to size limit";
        std::cout << std::endl;
      }
      TFile *file = new TFile(fileName.c_str(), "RECREATE");
      TTree *tree = new TTree("data", "data");
      TSmallEventData event;
      tree->Branch("Mod", &event.module, "Mod/b");
      tree->Branch("Ch", &event.channel, "Ch/b");
      tree->Branch("FineTS", &event.timeStampNs, "FineTS/D");
      tree->Branch("ChargeLong", &event.energy, "ChargeLong/s");
      tree->Branch("ChargeShort", &event.energyShort, "ChargeShort/S");
      tree->Branch("Signal", &event.waveform);
      for (const auto &data : localDataVec) {
        event.channel = data->channel;
        event.module = data->module;
        event.timeStampNs = data->timeStampNs;
        event.energy = data->energy;
        event.energyShort = data->energyShort;
        event.waveform = data->waveform;
        tree->Fill();
        delete data;
      }
      file->Write();
      file->Close();
      delete file;

      {
        std::lock_guard<std::mutex> lock(fFileMutex);
        std::cout << "Writing to " << fileName << " done" << std::endl;
      }

      localDataVec.clear();

    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void TDataRecorder::StartRecording()
{
  if (fRecording) return;
  fRecording = true;
  fFileVersion = 0;
  fLastWrite = std::chrono::system_clock::now();
  fRawDataQue.clear();
  fDataVec.clear();

  constexpr auto nThreads = 8;
  for (auto i = 0U; i < nThreads; i++) {
    fThreadPool.push_back(std::thread(&TDataRecorder::WritingThread, this));
    fThreadPool.push_back(std::thread(&TDataRecorder::ConvertingThread, this));
  }
}

void TDataRecorder::StopRecording()
{
  if (!fRecording) return;
  fRecording = false;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  PostProcess();

  for (auto &thread : fThreadPool) {
    if (thread.joinable()) thread.join();
  }
  fThreadPool.clear();
}

void TDataRecorder::PostProcess()
{
  // convert all data to root file
  // NEED refactoring
  // TOO REDUNDANT

  std::unique_ptr<DAQData_t> localRawData = nullptr;
  {
    std::lock_guard<std::mutex> lock(fRawDataQueMutex);
    if (!fRawDataQue.empty()) {
      while (true) {
        auto buf = std::move(fRawDataQue.front());
        fRawDataQue.pop_front();
        if (localRawData == nullptr) {
          localRawData = std::move(buf);
        } else {
          localRawData->insert(localRawData->end(),
                               std::make_move_iterator(buf->begin()),
                               std::make_move_iterator(buf->end()));
        }
        if (fRawDataQue.empty()) break;
      }
    }
  }

  std::vector<TSmallEventData *> localData;
  if (localRawData) {
    for (const auto &event : *localRawData) {
      TSmallEventData smallEvent;
      smallEvent.module = event->module;
      smallEvent.channel = event->channel;
      smallEvent.timeStampNs = event->timeStampNs;
      smallEvent.energy = event->energy;
      smallEvent.energyShort = event->energyShort;
      smallEvent.waveform.clear();
      if (event->waveformSize > 0)
        smallEvent.waveform.insert(smallEvent.waveform.end(),
                                   event->analogProbe1.begin(),
                                   event->analogProbe1.end());

      localData.emplace_back(new TSmallEventData(smallEvent));
    }
    localRawData.reset();
  }

  {
    std::lock_guard<std::mutex> lock(fDataVecMutex);
    if (localData.size() > 0)
      fDataVec.insert(fDataVec.end(), localData.begin(), localData.end());
    if (fDataVec.empty()) return;

    __gnu_parallel::sort(fDataVec.begin(), fDataVec.end(),
                         [](const auto &a, const auto &b) {
                           return a->timeStampNs < b->timeStampNs;
                         });
  }

  auto fileName = fFileName;
  fileName += std::string("_") + std::to_string(fFileVersion) + ".root";
  fFileVersion++;
  TFile *file = new TFile(fileName.c_str(), "RECREATE");
  TTree *tree = new TTree("data", "data");
  std::cout << "Writing to " << fileName << std::endl;

  TSmallEventData event;
  tree->Branch("Mod", &event.module, "Mod/b");
  tree->Branch("Ch", &event.channel, "Ch/b");
  tree->Branch("FineTS", &event.timeStampNs, "FineTS/D");
  tree->Branch("ChargeLong", &event.energy, "ChargeLong/s");
  tree->Branch("ChargeShort", &event.energyShort, "ChargeShort/S");
  tree->Branch("Signal", &event.waveform);
  for (const auto &data : fDataVec) {
    event.channel = data->channel;
    event.module = data->module;
    event.timeStampNs = data->timeStampNs;
    event.energy = data->energy;
    event.energyShort = data->energyShort;
    event.waveform = data->waveform;
    tree->Fill();
    delete data;
  }
  file->Write();
  file->Close();
  delete file;
  std::cout << "Writing to " << fileName << " done" << std::endl;
}