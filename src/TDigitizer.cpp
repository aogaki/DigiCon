#include "TDigitizer.hpp"

#include <unistd.h>

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

TDigitizer::TDigitizer() {}

TDigitizer::~TDigitizer() {}

void TDigitizer::LoadParameters(const std::string &filename)
{
  std::ifstream fin(filename);
  fin >> fParameters;
  fin.close();

  // Checking and sanitizing the parameters
  // NYI
}

uint32_t TDigitizer::GetNumberOfCh()
{
  std::string buf;
  GetParameter("/par/NumCh", buf);
  return static_cast<uint32_t>(std::stoi(buf));
}

uint32_t TDigitizer::GetDeltaT()
{
  std::string buf;
  GetParameter("/par/ADC_SamplRate", buf);
  return static_cast<uint32_t>(1000 / std::stoi(buf));
}

void TDigitizer::OpenDigitizer()
{
  auto URL = fParameters["URL"].get<std::string>();
  std::cout << "Open URL: " << URL << std::endl;
  int err = CAEN_FELib_Open(URL.c_str(), &fHandle);
  CheckError(err);

  SendCommand("/cmd/Reset");
  SendCommand("/cmd/CalibrateADC");

  fFW = fParameters["FW"].get<std::string>();

  auto tmp = std::stoi(fParameters["ModuleID"].get<std::string>());
  if (tmp > 255 || tmp < 0) {
    std::cerr << "Module ID out of range" << std::endl;
    exit(1);
  }
  fModNo = static_cast<uint8_t>(tmp);

  CheckDigitizer();
  PrintDigitizerInfo();
}

void TDigitizer::CloseDigitizer()
{
  SendCommand("/cmd/ClearData");
  int err = CAEN_FELib_Close(fHandle);
  CheckError(err);
}

void TDigitizer::CheckDigitizer() const
{
  std::string buf;
  GetParameter("/par/LicenseStatus", buf);
  std::cout << "License status: " << buf << std::endl;

  GetParameter("/par/fwtype", buf);
  std::cout << "Firmware type: " << buf << std::endl;
  if (fFW != buf) {
    std::cerr << "Firmware type mismatch" << std::endl;
    std::cerr << "Expected: " << fFW << "\tFW: " << buf << std::endl;
    exit(1);
  }

  auto SN = fParameters["SN"].get<std::string>();
  GetParameter("/par/serialnum", buf);
  std::cout << "Serial number: " << buf << std::endl;
  if (SN != buf) {
    std::cerr << "Serial number mismatch" << std::endl;
    std::cerr << "Expected: " << SN << "\tSN: " << buf << std::endl;
  }
}

void TDigitizer::PrintDigitizerInfo() const
{
  std::string value;
  GetParameter("/par/ModelName", value);
  std::cout << "Model name: " << value << std::endl;

  GetParameter("/par/ADC_Nbit", value);
  std::cout << "ADC bits: " << value << std::endl;

  GetParameter("/par/NumCh", value);
  std::cout << "Channels: " << value << std::endl;

  GetParameter("/par/ADC_SamplRate", value);  // in Msps
  std::cout << "ADC rate: " << value << " Msps" << std::endl;

  GetParameter("/par/AMC_FwVer", value);
  std::cout << "AMC firmware version: " << value << std::endl;

  GetParameter("/par/ROC_FwVer", value);
  std::cout << "ROC firmware version: " << value << std::endl;
}

void TDigitizer::ConfigDigitizer()
{
  // Module settings
  for (auto &modPar : fParameters["module_parameters"].items()) {
    auto path = modPar.value()["path"].get<std::string>();
    auto value = modPar.value()["value"].get<std::string>();
    // std::cout << "Path: " << path << "\tValue: " << value << std::endl;
    SetParameter(path, value);
  }

  // Channel settings
  for (auto &ch : fParameters["channel_parameters"].items()) {
    for (auto &chPar : ch.value().items()) {
      auto path = chPar.value()["path"].get<std::string>();
      auto value = chPar.value()["value"].get<std::string>();
      // std::cout << "Path: " << path << "\tValue: " << value << std::endl;
      SetParameter(path, value);
    }
  }

  // VTraces
  for (auto &vtrace : fParameters["trace_parameters"].items()) {
    for (auto &vtracePar : vtrace.value().items()) {
      auto path = vtracePar.value()["path"].get<std::string>();
      auto value = vtracePar.value()["value"].get<std::string>();
      // std::cout << "Path: " << path << "\tValue: " << value << std::endl;
      SetParameter(path, value);
    }
  }
}

void TDigitizer::ForceTrace() { SetParameter("/par/waveforms", "TRUE"); }

void TDigitizer::StartAcquisition()
{
  SendCommand("/cmd/ArmAcquisition");
  MakeNewEventsVec();

  fRunning = true;
  if (fFW == "DPP-PSD")
    fAcquisitionThread = std::thread(&TDigitizer::FetchEventsPSD, this);
  else if (fFW == "DPP-PHA")
    fAcquisitionThread = std::thread(&TDigitizer::FetchEventsPHA, this);
  else if (fFW == "SCOPE")
    fAcquisitionThread = std::thread(&TDigitizer::FetchEventsScope, this);
}

void TDigitizer::SendStartSignal()
{
  std::string startMode;
  GetParameter("/par/startmode", startMode);
  if (startMode == "START_MODE_FIRST_TRG") SendCommand("/cmd/SendSWTrigger");
}

void TDigitizer::StopAcquisition()
{
  SendCommand("/cmd/DisarmAcquisition");
  fRunning = false;

  if (fAcquisitionThread.joinable()) fAcquisitionThread.join();

  SendCommand("/cmd/ClearData");
}

void TDigitizer::SetDataFormat()
{
  // Define readout data structure
  nlohmann::json readDataJSON = fParameters["readout_data_format"];
  std::string readData = readDataJSON.dump();

  int err = 0;
  if (fFW == "DPP-PSD")
    err = CAEN_FELib_GetHandle(fHandle, "/endpoint/DPPPSD", &fReadDataHandle);
  else if (fFW == "DPP-PHA")
    err = CAEN_FELib_GetHandle(fHandle, "/endpoint/DPPPHA", &fReadDataHandle);
  else if (fFW == "SCOPE")
    err = CAEN_FELib_GetHandle(fHandle, "/endpoint/SCOPE", &fReadDataHandle);
  CheckError(err);
  err = CAEN_FELib_SetReadDataFormat(fReadDataHandle, readData.c_str());
  CheckError(err);
}

void TDigitizer::FetchEventsPSD()
{
  std::string buf;
  GetParameter("/par/reclen", buf);
  const auto recLen = static_cast<uint32_t>(std::stoi(buf));
  TEventData eventData(recLen);
  eventData.module = fModNo;
  std::vector<std::shared_ptr<TEventData>> eventBuffer;
  eventBuffer.reserve(10000);

  while (fRunning) {
    auto err = CAEN_FELib_ReadData(
        fReadDataHandle, fTimeOut, &eventData.channel, &eventData.timeStamp,
        &eventData.timeStampNs, &eventData.energy, &eventData.energyShort,
        &eventData.flags, eventData.analogProbe1.data(),
        &eventData.analogProbe1Type, eventData.analogProbe2.data(),
        &eventData.analogProbe2Type, eventData.digitalProbe1.data(),
        &eventData.digitalProbe1Type, eventData.digitalProbe2.data(),
        &eventData.digitalProbe2Type, &eventData.waveformSize,
        &eventData.eventSize);
    if (err == CAEN_FELib_Success && eventData.energy > 0) {
      eventBuffer.emplace_back(std::make_shared<TEventData>(eventData));
    }

    if (eventBuffer.size() > fEventThreshold || err != CAEN_FELib_Success) {
      std::lock_guard<std::mutex> lock(fEventsDataMutex);
      fEventsVec->insert(fEventsVec->end(),
                         std::make_move_iterator(eventBuffer.begin()),
                         std::make_move_iterator(eventBuffer.end()));
      eventBuffer.clear();
    }
  }

  std::lock_guard<std::mutex> lock(fEventsDataMutex);
  fEventsVec->insert(fEventsVec->end(),
                     std::make_move_iterator(eventBuffer.begin()),
                     std::make_move_iterator(eventBuffer.end()));
  eventBuffer.clear();
}

void TDigitizer::FetchEventsPHA()
{
  std::string buf;
  GetParameter("/par/reclen", buf);
  const auto recLen = static_cast<uint32_t>(std::stoi(buf));
  TEventData eventData(recLen);
  eventData.module = fModNo;
  eventData.energyShort = 0;
  std::vector<std::shared_ptr<TEventData>> eventBuffer;
  eventBuffer.reserve(10000);

  while (fRunning) {
    auto err = CAEN_FELib_ReadData(
        fReadDataHandle, fTimeOut, &eventData.channel, &eventData.timeStamp,
        &eventData.timeStampNs, &eventData.energy, &eventData.flags,
        eventData.analogProbe1.data(), &eventData.analogProbe1Type,
        eventData.analogProbe2.data(), &eventData.analogProbe2Type,
        eventData.digitalProbe1.data(), &eventData.digitalProbe1Type,
        eventData.digitalProbe2.data(), &eventData.digitalProbe2Type,
        &eventData.waveformSize, &eventData.eventSize);
    if (err == CAEN_FELib_Success && eventData.energy > 0) {
      eventBuffer.emplace_back(std::make_shared<TEventData>(eventData));
    }

    if (eventBuffer.size() > fEventThreshold || err != CAEN_FELib_Success) {
      std::lock_guard<std::mutex> lock(fEventsDataMutex);
      fEventsVec->insert(fEventsVec->end(),
                         std::make_move_iterator(eventBuffer.begin()),
                         std::make_move_iterator(eventBuffer.end()));
      eventBuffer.clear();
    }
  }

  std::lock_guard<std::mutex> lock(fEventsDataMutex);
  fEventsVec->insert(fEventsVec->end(),
                     std::make_move_iterator(eventBuffer.begin()),
                     std::make_move_iterator(eventBuffer.end()));
  eventBuffer.clear();
}

void TDigitizer::FetchEventsScope()
{
  std::string buf;
  GetParameter("/par/reclen", buf);
  const auto recLen = static_cast<uint32_t>(std::stoi(buf));
  TEventData eventData(recLen);
  eventData.module = fModNo;
  eventData.energy = 0;
  eventData.energyShort = 0;
  std::vector<std::shared_ptr<TEventData>> eventBuffer;
  eventBuffer.reserve(10000);

  uint64_t timeStamp;
  uint64_t timeStampNs;
  uint32_t triggerID;
  int16_t **waveform;
  GetParameter("/par/numch", buf);
  const auto nChs = static_cast<uint8_t>(std::stoi(buf));
  waveform = new int16_t *[nChs];
  for (auto i = 0; i < nChs; i++) {
    waveform[i] = new int16_t[recLen];
  }
  std::vector<std::size_t> waveformSize(nChs);
  uint16_t extra;
  uint8_t boardID;
  bool boardFail;
  uint32_t eventSize;

  while (fRunning) {
    int err = CAEN_FELib_ReadData(
        fReadDataHandle, fTimeOut, &timeStamp, &timeStampNs, &triggerID,
        waveform, &waveformSize[0], &extra, &boardID, &boardFail, &eventSize);
    if (err == CAEN_FELib_Success) {
      for (uint8_t iCh = 0; iCh < nChs; iCh++) {
        eventData.channel = iCh;
        eventData.timeStamp = timeStamp;
        eventData.timeStampNs = static_cast<double>(timeStampNs);  // dangerous
        eventData.analogProbe1 =
            std::vector<int16_t>(waveform[iCh], waveform[iCh] + recLen);
        eventBuffer.emplace_back(std::make_shared<TEventData>(eventData));
      }
    }

    if (eventBuffer.size() > fEventThreshold || err != CAEN_FELib_Success) {
      std::lock_guard<std::mutex> lock(fEventsDataMutex);
      fEventsVec->insert(fEventsVec->end(),
                         std::make_move_iterator(eventBuffer.begin()),
                         std::make_move_iterator(eventBuffer.end()));
      eventBuffer.clear();
    }
  }

  for (auto i = 0; i < nChs; i++) {
    delete[] waveform[i];
  }
  delete[] waveform;

  std::lock_guard<std::mutex> lock(fEventsDataMutex);
  fEventsVec->insert(fEventsVec->end(),
                     std::make_move_iterator(eventBuffer.begin()),
                     std::make_move_iterator(eventBuffer.end()));
  eventBuffer.clear();
}

std::shared_ptr<DAQData_t> TDigitizer::GetEvents()
{
  std::lock_guard<std::mutex> lock(fEventsDataMutex);
  auto buf = std::move(fEventsVec);
  MakeNewEventsVec();
  return buf;
}

void TDigitizer::MakeNewEventsVec()
{
  fEventsVec = std::make_shared<DAQData_t>();
  fEventsVec->reserve(1024 * 1024);
}

void TDigitizer::CheckError(int err) const
{
  auto errCode = static_cast<CAEN_FELib_ErrorCode>(err);
  if (errCode != CAEN_FELib_Success) {
    std::cout << "\x1b[31m";

    auto errName = std::string(32, '\0');
    CAEN_FELib_GetErrorName(errCode, errName.data());
    std::cerr << "Error code: " << errName << std::endl;

    auto errDesc = std::string(256, '\0');
    CAEN_FELib_GetErrorDescription(errCode, errDesc.data());
    std::cerr << "Error description: " << errDesc << std::endl;

    auto details = std::string(1024, '\0');
    CAEN_FELib_GetLastError(details.data());
    std::cerr << "Details: " << details << std::endl;

    std::cout << "\x1b[0m" << std::endl;
  }
}

bool TDigitizer::SendCommand(std::string path) const
{
  auto err = CAEN_FELib_SendCommand(fHandle, path.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}

bool TDigitizer::GetParameter(std::string path, std::string &value) const
{
  char buf[256];
  auto err = CAEN_FELib_GetValue(fHandle, path.c_str(), buf);
  CheckError(err);
  value = std::string(buf);

  return err == CAEN_FELib_Success;
}

bool TDigitizer::SetParameter(std::string path, std::string value) const
{
  auto err = CAEN_FELib_SetValue(fHandle, path.c_str(), value.c_str());
  CheckError(err);

  return err == CAEN_FELib_Success;
}