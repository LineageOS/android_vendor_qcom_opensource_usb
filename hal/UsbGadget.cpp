/*
 * Copyright (C) 2018-2021, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.usb.gadget@1.1-service-qti"

#include <android-base/file.h>
#include <android-base/properties.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <hidl/HidlTransportSupport.h>
#include <UsbGadgetCommon.h>
#include "UsbGadget.h"

#define ESOC_DEVICE_PATH "/sys/bus/esoc/devices"
#define SOC_MACHINE_PATH "/sys/devices/soc0/machine"
#define USB_CONTROLLER_PROP "vendor.usb.controller"
#define RNDIS_FUNC_NAME_PROP "vendor.usb.rndis.func.name"
#define RMNET_FUNC_NAME_PROP "vendor.usb.rmnet.func.name"
#define RMNET_INST_NAME_PROP "vendor.usb.rmnet.inst.name"
#define DPL_INST_NAME_PROP "vendor.usb.dpl.inst.name"
#define PERSIST_VENDOR_USB_PROP "persist.vendor.usb.config"

enum mdmType {
  INTERNAL,
  EXTERNAL,
  INTERNAL_EXTERNAL,
  NONE,
};

namespace android {
namespace hardware {
namespace usb {
namespace gadget {
namespace V1_1 {
namespace implementation {

using ::android::sp;
using ::android::base::GetProperty;
using ::android::base::SetProperty;
using ::android::base::WriteStringToFile;
using ::android::base::ReadFileToString;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::usb::gadget::V1_0::GadgetFunction;
using ::android::hardware::usb::gadget::V1_0::Status;
using ::android::hardware::usb::gadget::addAdb;
using ::android::hardware::usb::gadget::kDisconnectWaitUs;
using ::android::hardware::usb::gadget::linkFunction;
using ::android::hardware::usb::gadget::resetGadget;
using ::android::hardware::usb::gadget::setVidPid;
using ::android::hardware::usb::gadget::unlinkFunctions;

UsbGadget::UsbGadget(const char* const gadget)
    : mCurrentUsbFunctionsApplied(false),
      mMonitorFfs(gadget) {
  if (access(OS_DESC_PATH, R_OK) != 0)
    ALOGE("configfs setup not done yet");
}

Return<void> UsbGadget::getCurrentUsbFunctions(
    const sp<V1_0::IUsbGadgetCallback> &callback) {
  Return<void> ret = callback->getCurrentUsbFunctionsCb(
      mCurrentUsbFunctions, mCurrentUsbFunctionsApplied
                                ? Status::FUNCTIONS_APPLIED
                                : Status::FUNCTIONS_NOT_APPLIED);
  if (!ret.isOk())
    ALOGE("Call to getCurrentUsbFunctionsCb failed %s",
          ret.description().c_str());

  return Void();
}

Return<Status> UsbGadget::reset() {
  if (!WriteStringToFile("none", PULLUP_PATH)) {
    ALOGE("reset(): unable to clear pullup");
    return Status::ERROR;
  }

  return Status::SUCCESS;
}

V1_0::Status UsbGadget::tearDownGadget() {
  if (resetGadget() != Status::SUCCESS) return Status::ERROR;

  if (mMonitorFfs.isMonitorRunning())
    mMonitorFfs.reset();
  else
    ALOGE("mMonitor not running");

  return Status::SUCCESS;
}

static V1_0::Status validateAndSetVidPid(uint64_t functions) {
  V1_0::Status ret = Status::SUCCESS;
  switch (functions) {
    case static_cast<uint64_t>(GadgetFunction::ADB):
      ret = setVidPid("0x18d1", "0x4ee7");
      break;
    case static_cast<uint64_t>(GadgetFunction::MTP):
      ret = setVidPid("0x18d1", "0x4ee1");
      break;
    case GadgetFunction::ADB | GadgetFunction::MTP:
      ret = setVidPid("0x18d1", "0x4ee2");
      break;
    case static_cast<uint64_t>(GadgetFunction::RNDIS):
      ret = setVidPid("0x18d1", "0x4ee3");
      break;
    case GadgetFunction::ADB | GadgetFunction::RNDIS:
      ret = setVidPid("0x18d1", "0x4ee4");
      break;
    case static_cast<uint64_t>(GadgetFunction::PTP):
      ret = setVidPid("0x18d1", "0x4ee5");
      break;
    case GadgetFunction::ADB | GadgetFunction::PTP:
      ret = setVidPid("0x18d1", "0x4ee6");
      break;
    case static_cast<uint64_t>(GadgetFunction::MIDI):
      ret = setVidPid("0x18d1", "0x4ee8");
      break;
    case GadgetFunction::ADB | GadgetFunction::MIDI:
      ret = setVidPid("0x18d1", "0x4ee9");
      break;
    case static_cast<uint64_t>(GadgetFunction::ACCESSORY):
      ret = setVidPid("0x18d1", "0x2d00");
      break;
    case GadgetFunction::ADB | GadgetFunction::ACCESSORY:
      ret = setVidPid("0x18d1", "0x2d01");
      break;
    case static_cast<uint64_t>(GadgetFunction::AUDIO_SOURCE):
      ret = setVidPid("0x18d1", "0x2d02");
      break;
    case GadgetFunction::ADB | GadgetFunction::AUDIO_SOURCE:
      ret = setVidPid("0x18d1", "0x2d03");
      break;
    case GadgetFunction::ACCESSORY | GadgetFunction::AUDIO_SOURCE:
      ret = setVidPid("0x18d1", "0x2d04");
      break;
    case GadgetFunction::ADB | GadgetFunction::ACCESSORY |
	    GadgetFunction::AUDIO_SOURCE:
      ret = setVidPid("0x18d1", "0x2d05");
      break;
    default:
      ALOGE("Combination not supported");
      ret = Status::CONFIGURATION_NOT_SUPPORTED;
  }
  return ret;
}

static enum mdmType getModemType() {
  struct dirent* entry;
  enum mdmType mtype = INTERNAL;
  size_t pos_sda, pos_p, length;
  std::unique_ptr<DIR, int(*)(DIR*)> dir(opendir(ESOC_DEVICE_PATH), closedir);
  std::string esoc_name, path, soc_machine, esoc_dev_path = ESOC_DEVICE_PATH;

 /* On some platforms, /sys/bus/esoc/ director may not exists.*/
  if (dir == NULL)
      return mtype;

  while ((entry = readdir(dir.get())) != NULL) {
    if (entry->d_name[0] == '.')
      continue;
    path = esoc_dev_path + "/" + entry->d_name + "/esoc_name";
    if (ReadFileToString(path, &esoc_name)) {
      if (esoc_name.find("MDM") != std::string::npos ||
        esoc_name.find("SDX") != std::string::npos) {
        mtype = EXTERNAL;
        break;
      }
    }
  }
  if (ReadFileToString(SOC_MACHINE_PATH, &soc_machine)) {
    pos_sda = soc_machine.find("SDA");
    pos_p = soc_machine.find_last_of('P');
    length = soc_machine.length();
    if (pos_sda != std::string::npos || pos_p == length - 1) {
      mtype = mtype ? mtype : NONE;
      goto done;
    }
    if (mtype)
      mtype = INTERNAL_EXTERNAL;
  }
done:
  ALOGI("getModemType %d", mtype);
  return mtype;
}

V1_0::Status UsbGadget::setupFunctions(
    uint64_t functions, const sp<V1_0::IUsbGadgetCallback> &callback,
    uint64_t timeout) {
  bool ffsEnabled = false;
  int i = 0;
  enum mdmType mtype;
  std::string gadgetName = GetProperty(USB_CONTROLLER_PROP, "");
  std::string rndisFunc = GetProperty(RNDIS_FUNC_NAME_PROP, "");
  std::string rmnetFunc = GetProperty(RMNET_FUNC_NAME_PROP, "");
  std::string rmnetInst = GetProperty(RMNET_INST_NAME_PROP, "");
  std::string dplInst = GetProperty(DPL_INST_NAME_PROP, "");
  std::string vendorProp = GetProperty(PERSIST_VENDOR_USB_PROP, "");

  if (gadgetName.empty()) {
    ALOGE("UDC name not defined");
    return Status::ERROR;
  }

  if (rmnetInst.empty()) {
    ALOGE("rmnetinstance not defined");
    rmnetInst = "rmnet";
  }

  if (dplInst.empty()) {
    ALOGE("dplinstance not defined");
    dplInst = "dpl";
  }

  rmnetInst = rmnetFunc + "." + rmnetInst;
  dplInst = rmnetFunc + "." + dplInst;

  if (addGenericAndroidFunctions(&mMonitorFfs, functions, &ffsEnabled, &i) !=
      Status::SUCCESS)
    return Status::ERROR;

  mtype = getModemType();
  if ((functions & GadgetFunction::RNDIS) != 0) {
    if (functions & GadgetFunction::ADB) {
      if (mtype == EXTERNAL || mtype == INTERNAL_EXTERNAL) {
        ALOGI("esoc RNDIS default composition");
        if (linkFunction("diag.diag", i++)) return Status::ERROR;
        if (linkFunction("diag.diag_mdm", i++)) return Status::ERROR;
        if (linkFunction("qdss.qdss", i++)) return Status::ERROR;
        if (linkFunction("qdss.qdss_mdm", i++)) return Status::ERROR;
        if (linkFunction("cser.dun.0", i++)) return Status::ERROR;
        if (linkFunction(dplInst.c_str(), i++))
          return Status::ERROR;
        if (setVidPid("0x05c6", "0x90e7") != Status::SUCCESS)
          return Status::ERROR;
      } else if (mtype == INTERNAL) {
        ALOGI("RNDIS default composition");
        if (linkFunction("diag.diag", i++)) return Status::ERROR;
        if (linkFunction("qdss.qdss", i++)) return Status::ERROR;
        if (linkFunction("cser.dun.0", i++)) return Status::ERROR;
        if (linkFunction(dplInst.c_str(), i++))
          return Status::ERROR;
        if (setVidPid("0x05c6", "0x90e9") != Status::SUCCESS)
          return Status::ERROR;
      }
    }
  }

  /* override adb-only with additional QTI functions */
  if (i == 0 && functions & GadgetFunction::ADB) {
    /* vendor defined functions if any run from vendor rc file */
    if (!vendorProp.empty()) {
      ALOGI("enable vendor usb config composition");
      SetProperty("vendor.usb.config", vendorProp);
      return Status::SUCCESS;
    }

    if (mtype == EXTERNAL || mtype == INTERNAL_EXTERNAL) {
      ALOGI("esoc default composition");
      if (linkFunction("diag.diag", i++)) return Status::ERROR;
      if (linkFunction("diag.diag_mdm", i++)) return Status::ERROR;
      if (linkFunction("qdss.qdss", i++)) return Status::ERROR;
      if (linkFunction("qdss.qdss_mdm", i++)) return Status::ERROR;
      if (linkFunction("cser.dun.0", i++)) return Status::ERROR;
      if (linkFunction(dplInst.c_str(), i++))
        return Status::ERROR;
      if (linkFunction(rmnetInst.c_str(), i++))
        return Status::ERROR;
      if (setVidPid("0x05c6", "0x90e5") != Status::SUCCESS)
        return Status::ERROR;
    } else if (mtype == NONE) {
      ALOGI("enable APQ default composition");
      if (linkFunction("diag.diag", i++)) return Status::ERROR;
      if (setVidPid("0x05c6", "0x901d") != Status::SUCCESS)
        return Status::ERROR;
    } else {
      ALOGI("enable QC default composition");
      if (linkFunction("diag.diag", i++)) return Status::ERROR;
      if (linkFunction("cser.dun.0", i++)) return Status::ERROR;
      if (linkFunction(rmnetInst.c_str(), i++))
        return Status::ERROR;
      if (linkFunction(dplInst.c_str(), i++))
        return Status::ERROR;
      if (linkFunction("qdss.qdss", i++)) return Status::ERROR;
      if (setVidPid("0x05c6", "0x90db") != Status::SUCCESS)
        return Status::ERROR;
    }
  }

  // finally add ADB at the end if enabled
  if ((functions & GadgetFunction::ADB) != 0) {
    ffsEnabled = true;
    if (addAdb(&mMonitorFfs, &i) != Status::SUCCESS) return Status::ERROR;
  }

  // Pull up the gadget right away when there are no ffs functions.
  if (!ffsEnabled) {
    if (!WriteStringToFile(gadgetName, PULLUP_PATH)) return Status::ERROR;
    mCurrentUsbFunctionsApplied = true;
    if (callback)
      callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS);
    ALOGI("Gadget pullup without FFS fuctions");
    return Status::SUCCESS;
  }

  // Monitors the ffs paths to pull up the gadget when descriptors are written.
  // Also takes of the pulling up the gadget again if the userspace process
  // dies and restarts.
  mMonitorFfs.registerFunctionsAppliedCallback(
      [](bool functionsApplied, void *payload) {
        ((UsbGadget*)payload)->mCurrentUsbFunctionsApplied = functionsApplied;
      }, this);
  mMonitorFfs.startMonitor();

  ALOGI("Started monitor for FFS functions");

  if (callback) {
    bool gadgetPullup = mMonitorFfs.waitForPullUp(timeout);
    Return<void> ret = callback->setCurrentUsbFunctionsCb(
        functions, gadgetPullup ? Status::SUCCESS : Status::ERROR);
    if (!ret.isOk())
      ALOGE("setCurrentUsbFunctionsCb error %s", ret.description().c_str());
  }

  return Status::SUCCESS;
}

Return<void> UsbGadget::setCurrentUsbFunctions(
    uint64_t functions, const sp<V1_0::IUsbGadgetCallback> &callback,
    uint64_t timeout) {
  std::unique_lock<std::mutex> lk(mLockSetCurrentFunction);

  mCurrentUsbFunctions = functions;
  mCurrentUsbFunctionsApplied = false;

  // Unlink the gadget and stop the monitor if running.
  V1_0::Status status = tearDownGadget();
  if (status != Status::SUCCESS) {
    goto error;
  }

  // Leave the gadget pulled down to give time for the host to sense disconnect.
  usleep(kDisconnectWaitUs);

  if (functions == static_cast<uint64_t>(GadgetFunction::NONE)) {
    if (callback == NULL) return Void();
    Return<void> ret =
        callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS);
    if (!ret.isOk())
      ALOGE("Error while calling setCurrentUsbFunctionsCb %s",
            ret.description().c_str());
    return Void();
  }

  status = validateAndSetVidPid(functions);

  if (status != Status::SUCCESS) {
    goto error;
  }

  status = setupFunctions(functions, callback, timeout);
  if (status != Status::SUCCESS) {
    goto error;
  }

  ALOGI("Usb Gadget setcurrent functions called successfully");
  return Void();

error:
  ALOGI("Usb Gadget setcurrent functions failed");
  if (callback == NULL) return Void();
  Return<void> ret = callback->setCurrentUsbFunctionsCb(functions, status);
  if (!ret.isOk())
    ALOGE("Error while calling setCurrentUsbFunctionsCb %s",
          ret.description().c_str());
  return Void();
}
}  // namespace implementation
}  // namespace V1_1
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android

int main() {
  using android::base::GetProperty;
  using android::hardware::configureRpcThreadpool;
  using android::hardware::joinRpcThreadpool;
  using android::hardware::usb::gadget::V1_1::IUsbGadget;
  using android::hardware::usb::gadget::V1_1::implementation::UsbGadget;

  std::string gadgetName = GetProperty("persist.vendor.usb.controller",
      GetProperty(USB_CONTROLLER_PROP, ""));

  if (gadgetName.empty()) {
    ALOGE("UDC name not defined");
    return -1;
  }

  android::sp<IUsbGadget> service = new UsbGadget(gadgetName.c_str());

  configureRpcThreadpool(1, true /*callerWillJoin*/);
  android::status_t status = service->registerAsService();

  if (status != android::OK) {
    ALOGE("Cannot register USB Gadget HAL service");
    return 1;
  }

  ALOGI("QTI USB Gadget HAL Ready.");
  joinRpcThreadpool();
  // Under normal cases, execution will not reach this line.
  ALOGI("QTI USB Gadget HAL failed to join thread pool.");
  return 1;
}
