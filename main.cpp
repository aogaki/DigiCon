#include <TROOT.h>
#include <TSystem.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <random>

#include "TDataMonitor.hpp"
#include "TDataRecorder.hpp"
#include "TDataTaking.hpp"
#include "TDigitizer.hpp"
#include "TEventData.hpp"

enum class AppState { Quit, Reload, Continue };

AppState InputCheck()
{
  struct termios oldt, newt;
  char ch = -1;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if (ch == 'q' || ch == 'Q') {
    return AppState::Quit;
  } else if (ch == 'r' || ch == 'R') {
    return AppState::Reload;
  }

  return AppState::Continue;
}

std::unique_ptr<DAQData_t> GetFakeEvents(uint32_t nEvents = 10000)
{
  auto events = std::make_unique<DAQData_t>();
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint8_t> modDist(0, 7);
  std::uniform_int_distribution<uint8_t> chDist(0, 15);
  std::uniform_int_distribution<uint16_t> energyDist(1, 4095);
  std::uniform_int_distribution<int16_t> energyShortDist(1, 2047);

  static uint64_t counter = 0;

  for (auto i = 0U; i < nEvents; i++) {
    auto event = std::make_unique<TEventData>();
    event->module = modDist(gen);
    event->channel = chDist(gen);
    event->timeStampNs = counter++;
    event->energy = energyDist(gen);
    event->energyShort = energyShortDist(gen);
    event->waveformSize = 0;
    events->push_back(std::move(event));
  }

  return events;
}

int main(int argc, char *argv[])
{
  ROOT::EnableThreadSafety();

  bool forceTrace = false;
  bool useTestData = false;
  // bool useTestData = true;

  std::string configList = "configList";
  if (argc > 1) {
    for (auto i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-w") {
        forceTrace = true;
      } else if (std::string(argv[i]) == "-t") {
        useTestData = true;
      }
    }

    if (std::string(argv[argc - 1]).find('-') == std::string::npos)
      configList = argv[argc - 1];
  }

  auto daq = std::make_unique<TDataTaking>();
  if (useTestData == false) {
    daq->LoadConfigFileList(configList);
    daq->OpenDigitizers();
    if (forceTrace) {
      std::cout << "Trace ON" << std::endl;
      daq->ForceTrace();
    }
    daq->ConfigDigitizers();
  }

  auto monitor = std::make_unique<TDataMonitor>();
  if (useTestData) {
    std::cout << "Using test data" << std::endl;
    monitor->LoadChannelConf({64, 64, 64, 64, 64, 64, 64, 64});
    monitor->SetDeltaT({2, 2, 2, 2, 2, 2, 2, 2});
  } else {
    monitor->LoadChannelConf(daq->GetNumberOfCh());
    monitor->SetDeltaT(daq->GetDeltaT());
  }
  monitor->StartMonitor();

  auto recorder = std::make_unique<TDataRecorder>();
  recorder->SetFileName("test_data");
  recorder->SetSizeLimit(500 * 1024 * 1024);
  recorder->SetTimeLimit(30);  // minutes
  recorder->StartRecording();

  if (useTestData == false) {
    daq->StartAcquisition();
  }

  auto counter = 0UL;
  auto startTime = std::chrono::high_resolution_clock::now();
  while (true) {
    std::unique_ptr<DAQData_t> data;
    if (useTestData)
      data = std::move(GetFakeEvents(10000));
    else
      data = std::move(daq->GetData());

    if (data->size() > 0) {
      counter += data->size();
      auto copyData = std::make_unique<DAQData_t>();
      for (auto &event : *data) {
        auto copyEvent = std::make_unique<TEventData>();
        copyEvent->module = event->module;
        copyEvent->channel = event->channel;
        copyEvent->timeStampNs = event->timeStampNs;
        copyEvent->energy = event->energy;
        copyEvent->energyShort = event->energyShort;
        copyEvent->waveformSize = event->waveformSize;
        copyData->push_back(std::move(copyEvent));
      }
      monitor->SetData(std::move(data));
      recorder->SetData(std::move(copyData));
    }

    auto state = InputCheck();
    if (state == AppState::Quit) {
      break;
    } else if (state == AppState::Reload) {
      std::cout << "Reloading configuration files" << std::endl;
      monitor->StopMonitor();
      monitor->ClearHist();
      if (useTestData == false) {
        daq->StopAcquisition();
        daq->ConfigDigitizers();
        daq->StartAcquisition();
      }
      monitor->StartMonitor();
    }
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      endTime - startTime);
  std::cout << "Total time: " << duration.count() / 1000. << " s" << std::endl;
  std::cout << "Event rate: " << counter / (duration.count() / 1000.) << " Hz"
            << std::endl;

  if (useTestData == false) {
    daq->StopAcquisition();
    daq->CloseDigitizers();
  }
  recorder->StopRecording();
  monitor->StopMonitor();

  return 0;
}