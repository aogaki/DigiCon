#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>
#include <memory>

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
  auto digitizer = std::make_unique<TDigitizer>();
  auto fileName =
      "/home/aogaki/DAQ/DigiCon/build/Parameters_SN990_FWSCOPE.json";
  digitizer->LoadParameters(fileName);
  digitizer->OpenDigitizer();

  std::cout << "Configure" << std::endl;
  digitizer->ConfigDigitizer();
  digitizer->SetDataFormat();

  std::cout << "Start" << std::endl;
  digitizer->StartAcquisition();

  while (true) {
    auto data = digitizer->GetEvents();
    if (data->size() > 0)
      std::cout << "Data size: " << data->size() << " "
                << data->at(0)->timeStampNs << " " << int(data->at(0)->module)
                << std::endl;

    usleep(10000);

    auto state = InputCheck();
    if (state == AppState::Quit) {
      break;
    } else if (state == AppState::Reload) {
      // ;
    }
  }

  std::cout << "Stop" << std::endl;
  digitizer->StopAcquisition();

  std::cout << "Close" << std::endl;
  digitizer->CloseDigitizer();

  return 0;
}