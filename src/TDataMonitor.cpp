#include "TDataMonitor.hpp"

#include <TROOT.h>
#include <TSystem.h>

#include <chrono>
#include <iostream>

TDataMonitor::TDataMonitor()
{
  ROOT::EnableThreadSafety();

  fServer =
      std::make_unique<THttpServer>("http:8080?monitoring=1000;rw;noglobal");
}

TDataMonitor::~TDataMonitor()
{
  StopMonitor();
  gSystem->ProcessEvents();
  fServer.reset(nullptr);
}

void TDataMonitor::LoadChannelConf(const std::vector<uint32_t> &nChs)
{
  fModAndCh = nChs;
  InitHist();
  InitGraph();
  InitCanvas();
  RegisterHistCanvas();
}

void TDataMonitor::InitHist()
{
  fHist.clear();
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    std::vector<std::unique_ptr<TH1D>> mod;
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      auto hist = std::make_unique<TH1D>(
          Form("hist%02d%02d", iMod, iCh),
          Form("Module %d Channel %d", iMod, iCh), 30000, 0, 30000);
      hist->SetDirectory(nullptr);
      hist->SetXTitle("ADC");
      mod.push_back(std::move(hist));
    }
    fHist.push_back(std::move(mod));
  }
}

void TDataMonitor::InitGraph()
{
  fGraphAP1.clear();
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    std::vector<std::unique_ptr<TGraph>> mod;
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      auto graph = std::make_unique<TGraph>();
      graph->SetName(Form("graph%02d%02d", iMod, iCh));
      graph->SetTitle(Form("Module %d Channel %d", iMod, iCh));
      graph->SetMaximum(1 << 14);
      graph->SetMinimum(0);
      mod.push_back(std::move(graph));
    }
    fGraphAP1.push_back(std::move(mod));
  }

  fGraphAP2.clear();
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    std::vector<std::unique_ptr<TGraph>> mod;
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      auto graph = std::make_unique<TGraph>();
      graph->SetName(Form("graph%02d%02d", iMod, iCh));
      graph->SetTitle(Form("Module %d Channel %d", iMod, iCh));
      graph->SetMaximum(1 << 14);
      graph->SetMinimum(0);
      graph->SetLineColor(kRed);
      graph->SetMarkerColor(kRed);
      mod.push_back(std::move(graph));
    }
    fGraphAP2.push_back(std::move(mod));
  }

  fGraphDP1.clear();
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    std::vector<std::unique_ptr<TGraph>> mod;
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      auto graph = std::make_unique<TGraph>();
      graph->SetName(Form("graph%02d%02d", iMod, iCh));
      graph->SetTitle(Form("Module %d Channel %d", iMod, iCh));
      graph->SetMaximum(1 << 14);
      graph->SetMinimum(0);
      graph->SetLineColor(kGreen);
      graph->SetMarkerColor(kGreen);
      mod.push_back(std::move(graph));
    }
    fGraphDP1.push_back(std::move(mod));
  }

  fGraphDP2.clear();
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    std::vector<std::unique_ptr<TGraph>> mod;
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      auto graph = std::make_unique<TGraph>();
      graph->SetName(Form("graph%02d%02d", iMod, iCh));
      graph->SetTitle(Form("Module %d Channel %d", iMod, iCh));
      graph->SetMaximum(1 << 14);
      graph->SetMinimum(0);
      graph->SetLineColor(kBlue);
      graph->SetMarkerColor(kBlue);
      mod.push_back(std::move(graph));
    }
    fGraphDP2.push_back(std::move(mod));
  }
}

void TDataMonitor::InitCanvas()
{
  fCanvas.clear();
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    std::vector<std::unique_ptr<TCanvas>> mod;
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      auto canvas = std::make_unique<TCanvas>(
          Form("canvas%02d%02d", iMod, iCh),
          Form("Module %d Channel %d", iMod, iCh), 800, 600);
      mod.push_back(std::move(canvas));
    }
    fCanvas.push_back(std::move(mod));
  }
}

void TDataMonitor::RegisterHistCanvas()
{
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      fCanvas[iMod][iCh]->cd();
      fGraphAP1[iMod][iCh]->Draw("AL");
      fGraphAP2[iMod][iCh]->Draw("SAME");
      fGraphDP1[iMod][iCh]->Draw("SAME");
      fGraphDP2[iMod][iCh]->Draw("SAME");
      fCanvas[iMod][iCh]->SetGridx();
      fCanvas[iMod][iCh]->SetGridy();
    }
  }

  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    auto location = Form("/Module%02d", iMod);
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      fServer->Register(location, fHist[iMod][iCh].get());
      fServer->Register(location, fCanvas[iMod][iCh].get());
    }
  }
}

void TDataMonitor::SetData(std::shared_ptr<DAQData_t> data)
{
  {
    std::lock_guard<std::mutex> lock(fDataQueueMutex);
    fDataQueue.push_back(data);
  }
}

void TDataMonitor::FillingThread(uint32_t iThread)
{
  ROOT::EnableThreadSafety();

  // {
  //   std::lock_guard<std::mutex> lock(fThreadMutex);
  //   std::cout << "Thread " << iThread << " started" << std::endl;
  // }
  std::shared_ptr<DAQData_t> localData = nullptr;
  auto counter = 0;

  while (fMonitorRunning) {
    {
      std::lock_guard<std::mutex> lock(fDataQueueMutex);
      if (!fDataQueue.empty()) {
        localData = fDataQueue.front();
        fDataQueue.pop_front();
        counter++;
      }
    }

    if (localData) {
      std::vector<std::vector<bool>> drawFlag(fNMods,
                                              std::vector<bool>(fNChs, false));
      for (const auto &event : *localData) {
        if (fMonitorRunning == false) break;
        auto mod = event->module;
        auto ch = event->channel;
        if (mod >= fModAndCh.size() || ch >= fModAndCh[mod]) continue;

        fHist[mod][ch]->Fill(event->energy);

        if (event->waveformSize > 0) {
          if (drawFlag[mod][ch] == false) {
            drawFlag[mod][ch] = true;
            {
              std::lock_guard<std::mutex> lock(fAP1Mutex[mod][ch]);
              if (fGraphAP1[mod][ch]->GetN() == 0) {
                fGraphAP1[mod][ch]->Set(event->waveformSize);
              }
              auto *xAP1 = fGraphAP1[mod][ch]->GetX();
              auto *yAP1 = fGraphAP1[mod][ch]->GetY();
              for (auto i = 0U; i < event->waveformSize; i++) {
                xAP1[i] = i * fDeltaT[mod];
                yAP1[i] = event->analogProbe1[i];
              }
            }
            {
              std::lock_guard<std::mutex> lock(fAP2Mutex[mod][ch]);
              if (fGraphAP2[mod][ch]->GetN() == 0) {
                fGraphAP2[mod][ch]->Set(event->waveformSize);
              }
              auto *xAP2 = fGraphAP2[mod][ch]->GetX();
              auto *yAP2 = fGraphAP2[mod][ch]->GetY();
              for (auto i = 0U; i < event->waveformSize; i++) {
                xAP2[i] = i * fDeltaT[mod];
                yAP2[i] = event->analogProbe2[i];
              }
            }
            {
              std::lock_guard<std::mutex> lock(fDP1Mutex[mod][ch]);
              if (fGraphDP1[mod][ch]->GetN() == 0) {
                fGraphDP1[mod][ch]->Set(event->waveformSize);
              }
              auto *xDP1 = fGraphDP1[mod][ch]->GetX();
              auto *yDP1 = fGraphDP1[mod][ch]->GetY();
              for (auto i = 0U; i < event->waveformSize; i++) {
                xDP1[i] = i * fDeltaT[mod];
                yDP1[i] = event->digitalProbe1[i] * ((1 << 14) - 1000);
              }
            }
            {
              std::lock_guard<std::mutex> lock(fDP2Mutex[mod][ch]);
              if (fGraphDP2[mod][ch]->GetN() == 0) {
                fGraphDP2[mod][ch]->Set(event->waveformSize);
              }
              auto *xDP2 = fGraphDP2[mod][ch]->GetX();
              auto *yDP2 = fGraphDP2[mod][ch]->GetY();
              for (auto i = 0U; i < event->waveformSize; i++) {
                xDP2[i] = i * fDeltaT[mod];
                yDP2[i] = event->digitalProbe2[i] * ((1 << 14) - 1500);
              }
            }
          }
        }
      }

      localData.reset();
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  // {
  //   std::lock_guard<std::mutex> lock(fThreadMutex);
  //   std::cout << "Thread " << iThread << " stopped.  " << counter << std::endl;
  // }
}

void TDataMonitor::ROOTThread()
{
  ROOT::EnableThreadSafety();
  while (fMonitorRunning) {
    gSystem->ProcessEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void TDataMonitor::StartMonitor()
{
  fMonitorRunning = true;
  constexpr uint32_t nThreads = 16;
  fThreadPool.push_back(std::thread(&TDataMonitor::ROOTThread, this));
  for (auto i = 0U; i < nThreads; i++) {
    fThreadPool.push_back(std::thread(&TDataMonitor::FillingThread, this, i));
  }
}

void TDataMonitor::StopMonitor()
{
  fMonitorRunning = false;
  for (auto &thread : fThreadPool) {
    thread.join();
  }
  fThreadPool.clear();
}

void TDataMonitor::ClearHist()
{
  ROOT::EnableThreadSafety();
  for (auto iMod = 0U; iMod < fModAndCh.size(); iMod++) {
    for (auto iCh = 0U; iCh < fModAndCh[iMod]; iCh++) {
      fHist[iMod][iCh]->Reset("ICESM");
    }
  }
}