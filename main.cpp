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

#include "TDataMonitor.hpp"
#include "TDataTaking.hpp"
#include "TDigitizer.hpp"

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

int main(int argc, char *argv[])
{
  ROOT::EnableThreadSafety();

  bool forceTrace = false;

  std::string configList = "configList";
  if (argc > 1) {
    for (auto i = 1; i < argc - 1; i++) {
      if (std::string(argv[i]) == "-w") {
        forceTrace = true;
      }
    }

    configList = argv[argc - 1];
  }

  auto daq = std::make_unique<TDataTaking>();
  daq->LoadConfigFileList(configList);
  daq->OpenDigitizers();

  if (forceTrace) {
    std::cout << "Trace ON" << std::endl;
    daq->ForceTrace();
  }
  daq->ConfigDigitizers();

  auto monitor = std::make_unique<TDataMonitor>();
  monitor->LoadChannelConf(daq->GetNumberOfCh());
  monitor->SetDeltaT(daq->GetDeltaT());
  monitor->StartMonitor();

  daq->StartAcquisition();

  auto counter = 0UL;
  auto startTime = std::chrono::high_resolution_clock::now();
  while (true) {
    auto data = daq->GetData();
    if (data->size() > 0) {
      counter += data->size();
      monitor->SetData(data);
    }

    auto state = InputCheck();
    if (state == AppState::Quit) {
      break;
    } else if (state == AppState::Reload) {
      std::cout << "Reloading configuration files" << std::endl;
      monitor->StopMonitor();
      monitor->ClearHist();
      daq->StopAcquisition();
      daq->ConfigDigitizers();
      monitor->StartMonitor();
      daq->StartAcquisition();
    }
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
  std::cout << "Total time: " << duration.count() << " s" << std::endl;
  std::cout << "Event rate: " << counter / duration.count() << " Hz"
            << std::endl;

  monitor->StopMonitor();
  daq->StopAcquisition();
  daq->CloseDigitizers();

  return 0;
}