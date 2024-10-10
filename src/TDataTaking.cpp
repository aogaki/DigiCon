#include "TDataTaking.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TDataTaking::TDataTaking() {}

TDataTaking::~TDataTaking() {}

std::vector<uint32_t> TDataTaking::GetNumberOfCh()
{
  std::vector<uint32_t> numberOfHW;
  for (const auto &digitizer : fDigitizers) {
    numberOfHW.push_back(digitizer->GetNumberOfCh());
  }
  return numberOfHW;
}

std::vector<uint32_t> TDataTaking::GetDeltaT()
{
  std::vector<uint32_t> deltaT;
  for (const auto &digitizer : fDigitizers) {
    deltaT.push_back(digitizer->GetDeltaT());
  }
  return deltaT;
}

void TDataTaking::LoadConfigFileList(const std::string &listName)
{
  std::ifstream fin(listName);
  std::string line;
  while (std::getline(fin, line)) {
    if (line[0] == '#') continue;
    fConfigFileList.push_back(line);
  }
  fin.close();

  std::cout << fConfigFileList.size() << " configuration files will be loaded."
            << std::endl;
  for (const auto &configFile : fConfigFileList) {
    std::cout << configFile << std::endl;

    if (std::filesystem::exists(configFile)) {
      std::cout << "File exists" << std::endl;
    } else {
      std::cerr << "File does not exist" << std::endl;
      exit(1);
    }
  }

  LoadConfigFiles();
}

void TDataTaking::LoadConfigFiles()
{
  if (fDigitizers.size() == 0) {
    for (const auto &configFile : fConfigFileList) {
      auto digitizer = std::make_unique<TDigitizer>();
      digitizer->LoadParameters(configFile);
      fDigitizers.push_back(std::move(digitizer));
    }
  } else {
    for (auto i = 0U; i < fDigitizers.size(); i++) {
      fDigitizers[i]->LoadParameters(fConfigFileList[i]);
    }
  }
}

void TDataTaking::OpenDigitizers()
{
  // By CAEN documents, open and close should be done in the same thread
  for (auto &digitizer : fDigitizers) {
    digitizer->OpenDigitizer();
  }
}

void TDataTaking::CloseDigitizers()
{
  // By CAEN documents, open and close should be done in the same thread
  for (auto &digitizer : fDigitizers) {
    digitizer->CloseDigitizer();
  }
}

void TDataTaking::ConfigDigitizers()
{
  LoadConfigFiles();
  std::vector<std::thread> threads;
  for (auto &digitizer : fDigitizers) {
    threads.emplace_back([&digitizer, this] {
      digitizer->ConfigDigitizer();
      if (fForceTrace) digitizer->ForceTrace();
      digitizer->SetDataFormat();
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
}

void TDataTaking::StartAcquisition()
{
  ResetEventsVec();

  std::vector<std::thread> threads;
  for (auto &digitizer : fDigitizers) {
    threads.emplace_back([&digitizer] { digitizer->StartAcquisition(); });
  }
  for (auto &thread : threads) {
    thread.join();
  }

  fRunning = true;
  fAcquisitionThreads.clear();
  fAcquisitionThreads.emplace_back(&TDataTaking::FetchingData, this);

  for (auto &digitizer : fDigitizers) {
    digitizer->SendStartSignal();
  }
}

void TDataTaking::StopAcquisition()
{
  std::vector<std::thread> threads;
  for (auto &digitizer : fDigitizers) {
    threads.emplace_back([&digitizer] { digitizer->StopAcquisition(); });
  }
  for (auto &thread : threads) {
    thread.join();
  }

  fRunning = false;
  for (auto &thread : fAcquisitionThreads) {
    thread.join();
  }
}

void TDataTaking::ResetEventsVec()
{
  fEventsVec.reset(new std::vector<std::unique_ptr<TEventData>>);
  fEventsVec->reserve(1024 * 1024);
}

std::unique_ptr<DAQData_t> TDataTaking::GetData()
{
  std::lock_guard<std::mutex> lock(fEventsVecMutex);
  auto buf = std::move(fEventsVec);
  ResetEventsVec();
  return buf;
}

void TDataTaking::FetchingData()
{
  std::unique_ptr<DAQData_t> localEventsVec;
  localEventsVec.reset(new std::vector<std::unique_ptr<TEventData>>);

  while (fRunning) {
    for (auto &digitizer : fDigitizers) {
      auto data = digitizer->GetEvents();
      localEventsVec->insert(localEventsVec->end(),
                             std::make_move_iterator(data->begin()),
                             std::make_move_iterator(data->end()));
    }

    if (localEventsVec->size() > 0) {
      {
        std::lock_guard<std::mutex> lock(fEventsVecMutex);
        fEventsVec->insert(fEventsVec->end(),
                           std::make_move_iterator(localEventsVec->begin()),
                           std::make_move_iterator(localEventsVec->end()));
      }
      localEventsVec->clear();
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(fSleepTime));
    }
  }
}
