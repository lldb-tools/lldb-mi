//===-- MICmnLLDBDebuggerHandleEvents.cpp -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Third party headers:
#include "lldb/API/SBAddress.h"
#include "lldb/API/SBBreakpoint.h"
#include "lldb/API/SBCommandInterpreter.h"
#include "lldb/API/SBCommandReturnObject.h"
#include "lldb/API/SBEvent.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBStream.h"
#include "lldb/API/SBTarget.h"
#include "lldb/API/SBThread.h"
#include "lldb/API/SBUnixSignals.h"
#include "llvm/Support/Compiler.h"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif // _WIN32

// In-house headers:
#include "MICmnLLDBDebugSessionInfo.h"
#include "MICmnLLDBDebugger.h"
#include "MICmnLLDBDebuggerHandleEvents.h"
#include "MICmnLog.h"
#include "MICmnMIOutOfBandRecord.h"
#include "MICmnMIResultRecord.h"
#include "MICmnMIValueConst.h"
#include "MICmnMIValueList.h"
#include "MICmnResources.h"
#include "MICmnStreamStderr.h"
#include "MICmnStreamStdout.h"
#include "MIDriver.h"
#include "MIUtilDebug.h"
#include "Platform.h"

#include <algorithm>

//++
// Details: CMICmnLLDBDebuggerHandleEvents constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmnLLDBDebuggerHandleEvents::CMICmnLLDBDebuggerHandleEvents() {}

//++
// Details: CMICmnLLDBDebuggerHandleEvents destructor.
// Type:    Overridable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmnLLDBDebuggerHandleEvents::~CMICmnLLDBDebuggerHandleEvents() {
  Shutdown();
}

//++
// Details: Initialize resources for *this broadcaster object.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::Initialize() {
  m_clientUsageRefCnt++;

  if (m_bInitialized)
    return MIstatus::success;

  m_bInitialized = MIstatus::success;
  m_bSignalsInitialized = false;
  m_SIGINT = 0;
  m_SIGSTOP = 0;
  m_SIGSEGV = 0;
  m_SIGTRAP = 0;

  return m_bInitialized;
}

//++
// Details: Release resources for *this broadcaster object.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::Shutdown() {
  if (--m_clientUsageRefCnt > 0)
    return MIstatus::success;

  if (!m_bInitialized)
    return MIstatus::success;

  m_bInitialized = false;

  return MIstatus::success;
}

//++
// Details: Interpret the event object to ascertain the action to take or
// information to
//          to form and put in a MI Out-of-band record object which is given to
//          stdout.
// Type:    Method.
// Args:    vEvent          - (R) An LLDB broadcast event.
//          vrbHandledEvent - (W) True - event handled, false = not handled.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEvent(const lldb::SBEvent &vEvent,
                                                 bool &vrbHandledEvent) {
  bool bOk = MIstatus::success;
  vrbHandledEvent = false;

  if (lldb::SBProcess::EventIsProcessEvent(vEvent)) {
    vrbHandledEvent = true;
    bOk = HandleEventSBProcess(vEvent);
  } else if (lldb::SBBreakpoint::EventIsBreakpointEvent(vEvent)) {
    vrbHandledEvent = true;
    bOk = HandleEventSBBreakPoint(vEvent);
  } else if (lldb::SBWatchpoint::EventIsWatchpointEvent(vEvent)) {
    vrbHandledEvent = true;
    bOk = HandleEventSBWatchpoint(vEvent);
  } else if (lldb::SBThread::EventIsThreadEvent(vEvent)) {
    vrbHandledEvent = true;
    bOk = HandleEventSBThread(vEvent);
  } else if (lldb::SBTarget::EventIsTargetEvent(vEvent)) {
    vrbHandledEvent = true;
    bOk = HandleEventSBTarget(vEvent);
  } else if (lldb::SBCommandInterpreter::EventIsCommandInterpreterEvent(
                 vEvent)) {
    vrbHandledEvent = true;
    bOk = HandleEventSBCommandInterpreter(vEvent);
  }

  return bOk;
}

//++
// Details: Handle a LLDB SBProcess event.
// Type:    Method.
// Args:    vEvent          - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBProcess(
    const lldb::SBEvent &vEvent) {
  bool bOk = MIstatus::success;

  const char *pEventType = "";
  const MIuint nEventType = vEvent.GetType();
  switch (nEventType) {
  case lldb::SBProcess::eBroadcastBitInterrupt:
    pEventType = "eBroadcastBitInterrupt";
    break;
  case lldb::SBProcess::eBroadcastBitProfileData:
    pEventType = "eBroadcastBitProfileData";
    break;
  case lldb::SBProcess::eBroadcastBitStructuredData:
    pEventType = "eBroadcastBitStructuredData";
    break;
  case lldb::SBProcess::eBroadcastBitStateChanged:
    pEventType = "eBroadcastBitStateChanged";
    bOk = HandleProcessEventBroadcastBitStateChanged(vEvent);
    break;
  case lldb::SBProcess::eBroadcastBitSTDERR:
    pEventType = "eBroadcastBitSTDERR";
    bOk = GetProcessStderr();
    break;
  case lldb::SBProcess::eBroadcastBitSTDOUT:
    pEventType = "eBroadcastBitSTDOUT";
    bOk = GetProcessStdout();
    break;
  default: {
    const CMIUtilString msg(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_UNKNOWN_EVENT),
                              "SBProcess", (MIuint)nEventType));
    SetErrorDescription(msg);
    return MIstatus::failure;
  }
  }
  m_pLog->WriteLog(CMIUtilString::Format(
      "##### An SB Process event occurred: %s", pEventType));

  return bOk;
}

//++
// Details: Handle a LLDB SBBreakpoint event.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBBreakPoint(
    const lldb::SBEvent &vEvent) {
  bool bOk = MIstatus::success;

  const char *pEventType = "";
  const lldb::BreakpointEventType eEvent =
      lldb::SBBreakpoint::GetBreakpointEventTypeFromEvent(vEvent);
  lldb::SBBreakpoint vBreakpoint =
      lldb::SBBreakpoint::GetBreakpointFromEvent(vEvent);
  switch (eEvent) {
  case lldb::eBreakpointEventTypeThreadChanged:
    pEventType = "eBreakpointEventTypeThreadChanged";
    break;
  case lldb::eBreakpointEventTypeLocationsRemoved:
    pEventType = "eBreakpointEventTypeLocationsRemoved";
    break;
  case lldb::eBreakpointEventTypeInvalidType:
    pEventType = "eBreakpointEventTypeInvalidType";
    break;
  case lldb::eBreakpointEventTypeLocationsAdded:
    pEventType = "eBreakpointEventTypeLocationsAdded";
    bOk = HandleEventSBBreakpointLocationsAdded(vEvent);
    break;
  case lldb::eBreakpointEventTypeAdded:
    pEventType = "eBreakpointEventTypeAdded";
    bOk = HandleEventStoppointAdded(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeRemoved:
    pEventType = "eBreakpointEventTypeRemoved";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    bOk &= RemoveStoppointInfo(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeLocationsResolved:
    pEventType = "eBreakpointEventTypeLocationsResolved";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeEnabled:
    pEventType = "eBreakpointEventTypeEnabled";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeDisabled:
    pEventType = "eBreakpointEventTypeDisabled";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeCommandChanged:
    pEventType = "eBreakpointEventTypeCommandChanged";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeConditionChanged:
    pEventType = "eBreakpointEventTypeConditionChanged";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeIgnoreChanged:
    pEventType = "eBreakpointEventTypeIgnoreChanged";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    break;
  case lldb::eBreakpointEventTypeAutoContinueChanged:
    pEventType = "eBreakpointEventTypeAutoContinueChanged";
    bOk = HandleEventStoppointCmn(vBreakpoint);
    break;
  }
  m_pLog->WriteLog(CMIUtilString::Format(
      "##### An SB Breakpoint event occurred: %s", pEventType));

  return bOk;
}

bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBWatchpoint(
    const lldb::SBEvent &vEvent) {
  bool bOk = MIstatus::success;

  const char *pEventType = "";
  const lldb::WatchpointEventType eEvent =
      lldb::SBWatchpoint::GetWatchpointEventTypeFromEvent(vEvent);
  lldb::SBWatchpoint vWatchpoint =
      lldb::SBWatchpoint::GetWatchpointFromEvent(vEvent);
  switch (eEvent) {
  case lldb::eWatchpointEventTypeInvalidType:
    pEventType = "eWatchpointEventTypeInvalidType";
    bOk = MIstatus::failure;
    break;
  case lldb::eWatchpointEventTypeAdded:
    pEventType = "eWatchpointEventTypeAdded";
    bOk = HandleEventStoppointAdded(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeRemoved:
    pEventType = "eWatchpointEventTypeRemoved";
    bOk = HandleEventStoppointCmn(vWatchpoint);
    bOk &= RemoveStoppointInfo(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeEnabled:
    pEventType = "eWatchpointEventTypeEnabled";
    bOk = HandleEventStoppointCmn(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeDisabled:
    pEventType = "eWatchpointEventTypeDisabled";
    bOk = HandleEventStoppointCmn(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeCommandChanged:
    pEventType = "eWatchpointEventTypeCommandChanged";
    bOk = HandleEventStoppointCmn(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeConditionChanged:
    pEventType = "eWatchpointEventTypeConditionChanged";
    bOk = HandleEventStoppointCmn(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeIgnoreChanged:
    pEventType = "eWatchpointEventTypeIgnoreChanged";
    bOk = HandleEventStoppointCmn(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeThreadChanged:
    pEventType = "eWatchpointEventTypeThreadChanged";
    bOk = HandleEventStoppointCmn(vWatchpoint);
    break;
  case lldb::eWatchpointEventTypeTypeChanged:
    pEventType = "eWatchpointEventTypeTypeChanged";
    bOk = HandleEventStoppointCmn(vWatchpoint);
  }
  m_pLog->WriteLog(CMIUtilString::Format(
      "##### An SB Watchpoint event occurred: %s", pEventType));

  return bOk;
}

//++
// Details: Handle a LLDB SBBreakpoint event.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBBreakpointLocationsAdded(
    const lldb::SBEvent &vEvent) {
  const MIuint nLoc =
      lldb::SBBreakpoint::GetNumBreakpointLocationsFromEvent(vEvent);
  if (nLoc == 0)
    return MIstatus::success;

  lldb::SBBreakpoint breakpoint =
      lldb::SBBreakpoint::GetBreakpointFromEvent(vEvent);
  const CMIUtilString plural((nLoc == 1) ? "" : "s");
  const CMIUtilString msg(
      CMIUtilString::Format("%d location%s added to breakpoint %d", nLoc,
                            plural.c_str(), breakpoint.GetID()));

  return TextToStdout(msg);
}

//++
// Details: Handle a LLDB SBBreakpoint or SBWatchpoint event.
// Type:    Method.
// Args:    vrStoppoint  - (R) An LLDB breakpoint or watchpoint.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
template <class T, class>
bool CMICmnLLDBDebuggerHandleEvents::HandleEventStoppointCmn(T &vrStoppoint) {
  if (!vrStoppoint.IsValid())
    return MIstatus::success;

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());
  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.GetStoppointInfo(vrStoppoint, sStoppointInfo)) {
    const char *ptType =
        std::is_same<std::remove_cv_t<T>, lldb::SBBreakpoint>::value
            ? "breakpoint"
            : "watchpoint";
    SetErrorDescription(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_STOPPOINT_INFO_GET),
                              __func__, ptType, vrStoppoint.GetID()));
    return MIstatus::failure;
  }

  // CODETAG_LLDB_BREAKPOINT_CREATION
  // This is in a worker thread
  // Add more breakpoint information or overwrite existing information
  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfoRec;
  if (!rSessionInfo.RecordStoppointInfoGet(sStoppointInfo.m_nMiId,
                                           sStoppointInfoRec)) {
    SetErrorDescription(CMIUtilString::Format(
        MIRSRC(IDS_LLDBOUTOFBAND_ERR_STOPPOINT_NOTFOUND), __func__,
        static_cast<uint64_t>(sStoppointInfo.m_nMiId)));
    return MIstatus::failure;
  }
  sStoppointInfo.m_bDisp = sStoppointInfoRec.m_bDisp;
  sStoppointInfo.m_bEnabled = vrStoppoint.IsEnabled();
  sStoppointInfo.m_bHaveArgOptionThreadGrp = false;
  sStoppointInfo.m_strOptThrdGrp = "";
  sStoppointInfo.m_nTimes = vrStoppoint.GetHitCount();
  sStoppointInfo.m_strOrigLoc = sStoppointInfoRec.m_strOrigLoc;
  sStoppointInfo.m_nIgnore = sStoppointInfoRec.m_nIgnore;
  sStoppointInfo.m_bPending = sStoppointInfoRec.m_bPending;
  sStoppointInfo.m_bCondition = sStoppointInfoRec.m_bCondition;
  sStoppointInfo.m_strCondition = sStoppointInfoRec.m_strCondition;
  sStoppointInfo.m_bBreakpointThreadId =
      sStoppointInfoRec.m_bBreakpointThreadId;
  sStoppointInfo.m_nBreakpointThreadId =
      sStoppointInfoRec.m_nBreakpointThreadId;
  sStoppointInfo.m_watchpointExpr = sStoppointInfoRec.m_watchpointExpr;
  sStoppointInfo.m_watchpointRead = sStoppointInfoRec.m_watchpointRead;
  sStoppointInfo.m_watchpointWrite = sStoppointInfoRec.m_watchpointWrite;

  // MI print
  // "=breakpoint-modified,bkpt={number=\"%d\",type=\"breakpoint\",disp=\"%s\",enabled=\"%c\",addr=\"0x%016"
  // PRIx64 "\",
  // func=\"%s\",file=\"%s\",fullname=\"%s/%s\",line=\"%d\",times=\"%d\",original-location=\"%s\"}"
  CMICmnMIValueTuple miValueTuple;
  if (!rSessionInfo.MIResponseFormBreakpointInfo(sStoppointInfo,
                                                 miValueTuple)) {
    SetErrorDescription(CMIUtilString::Format(
        MIRSRC(IDS_LLDBOUTOFBAND_ERR_FORM_MI_RESPONSE), __func__));
    return MIstatus::failure;
  }

  const CMICmnMIValueResult miValueResultC("bkpt", miValueTuple);
  const CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_BreakPointModified, miValueResultC);
  bool bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
  bOk = bOk && CMICmnStreamStdout::WritePrompt();

  return bOk;
}

template bool
CMICmnLLDBDebuggerHandleEvents::HandleEventStoppointCmn(lldb::SBBreakpoint &);
template bool
CMICmnLLDBDebuggerHandleEvents::HandleEventStoppointCmn(lldb::SBWatchpoint &);

template <class T>
static void FillInPointTypeDependentInfo(
    T &vrStoppoint, CMICmnLLDBDebugSessionInfo::SStoppointInfo &vrInfo);

template <>
void FillInPointTypeDependentInfo(
    lldb::SBBreakpoint &vrStoppoint,
    CMICmnLLDBDebugSessionInfo::SStoppointInfo &vrInfo) {
  vrInfo.m_bDisp = vrStoppoint.IsOneShot();
  vrInfo.m_bBreakpointThreadId = vrStoppoint.GetThreadID() != 0;
  vrInfo.m_nBreakpointThreadId = vrStoppoint.GetThreadID();
}

template <>
void FillInPointTypeDependentInfo(
    lldb::SBWatchpoint &vrStoppoint,
    CMICmnLLDBDebugSessionInfo::SStoppointInfo &vrInfo) {
  // The original expression is known by `break-watch` command implementation
  // only. So, if the event is handled before the command finished then the
  // command will put a real value later. In case a watchpoint is added via
  // typing a command directly into the console, we cannot determine the
  // original expression at all. So, we use the address as an expression here.
  vrInfo.m_watchpointExpr = CMIUtilString::Format(
      "0x%08" PRIx64, static_cast<uint64_t>(vrStoppoint.GetWatchAddress()));

  // There is no way to figure out the type of the watchpoint by the watchpoint
  // itself, so here the default values are set and the real values must be set
  // by `break-watch` command implementation.
  vrInfo.m_watchpointRead = false;
  vrInfo.m_watchpointWrite = true;

  vrInfo.m_bDisp = false;
}

//++
// Details: Handle a LLDB stoppoint added event.
//          Add more stoppoint information or overwrite existing information.
//          Normally a break point session info objects exists by now when an MI
//          command
//          was issued to insert a break so the retrieval would normally always
//          succeed
//          however should a user type "b main" into a console then LLDB will
//          create a
//          breakpoint directly, hence no MI command, hence no previous record
//          of the
//          breakpoint so RecordStoppointInfoGet() will fail. We still get the
//          event though so need to create a breakpoint info object here and
//          send appropriate MI response.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
template <class T, class>
bool CMICmnLLDBDebuggerHandleEvents::HandleEventStoppointAdded(T &vrStoppoint) {
  if (!vrStoppoint.IsValid())
    return MIstatus::failure;

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());
  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.GetStoppointInfo(vrStoppoint, sStoppointInfo)) {
    const char *ptType =
        std::is_same<std::remove_cv_t<T>, lldb::SBBreakpoint>::value
            ? "breakpoint"
            : "watchpoint";
    SetErrorDescription(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_STOPPOINT_INFO_GET),
                              __func__, ptType, vrStoppoint.GetID()));
    return MIstatus::failure;
  }

  // CODETAG_LLDB_BREAKPOINT_CREATION
  // This is in a worker thread
  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfoRec;
  const bool bBreakpointExistAlready = rSessionInfo.RecordStoppointInfoGet(
      sStoppointInfo.m_nMiId, sStoppointInfoRec);
  if (bBreakpointExistAlready) {
    // Update breakpoint information object
    sStoppointInfo.m_bDisp = sStoppointInfoRec.m_bDisp;
    sStoppointInfo.m_bEnabled = vrStoppoint.IsEnabled();
    sStoppointInfo.m_bHaveArgOptionThreadGrp = false;
    sStoppointInfo.m_strOptThrdGrp.clear();
    sStoppointInfo.m_nTimes = vrStoppoint.GetHitCount();
    sStoppointInfo.m_strOrigLoc = sStoppointInfoRec.m_strOrigLoc;
    sStoppointInfo.m_nIgnore = sStoppointInfoRec.m_nIgnore;
    sStoppointInfo.m_bPending = sStoppointInfoRec.m_bPending;
    sStoppointInfo.m_bCondition = sStoppointInfoRec.m_bCondition;
    sStoppointInfo.m_strCondition = sStoppointInfoRec.m_strCondition;
    sStoppointInfo.m_bBreakpointThreadId =
        sStoppointInfoRec.m_bBreakpointThreadId;
    sStoppointInfo.m_nBreakpointThreadId =
        sStoppointInfoRec.m_nBreakpointThreadId;
    sStoppointInfo.m_watchpointExpr = sStoppointInfoRec.m_watchpointExpr;
    sStoppointInfo.m_watchpointRead = sStoppointInfoRec.m_watchpointRead;
    sStoppointInfo.m_watchpointWrite = sStoppointInfoRec.m_watchpointWrite;
  } else {
    // Create a breakpoint information object
    sStoppointInfo.m_bEnabled = vrStoppoint.IsEnabled();
    sStoppointInfo.m_bHaveArgOptionThreadGrp = false;
    sStoppointInfo.m_strOptThrdGrp.clear();
    sStoppointInfo.m_strOrigLoc = CMIUtilString::Format(
        "%s:%d", sStoppointInfo.m_fileName.c_str(), sStoppointInfo.m_nLine);
    sStoppointInfo.m_nIgnore = vrStoppoint.GetIgnoreCount();
    sStoppointInfo.m_bPending = false;
    const char *pStrCondition = vrStoppoint.GetCondition();
    sStoppointInfo.m_bCondition = pStrCondition != nullptr;
    sStoppointInfo.m_strCondition =
        (pStrCondition != nullptr) ? pStrCondition : "??";
    FillInPointTypeDependentInfo(vrStoppoint, sStoppointInfo);
  }

  CMICmnMIValueTuple miValueTuple;
  if (!rSessionInfo.MIResponseFormBreakpointInfo(sStoppointInfo,
                                                 miValueTuple)) {
    SetErrorDescription(CMIUtilString::Format(
        MIRSRC(IDS_LLDBOUTOFBAND_ERR_FORM_MI_RESPONSE), __func__));
    return MIstatus::failure;
  }

  bool bOk = MIstatus::success;
  if (bBreakpointExistAlready) {
    // MI print
    // "=breakpoint-modified,bkpt={number=\"%d\",type=\"breakpoint\",disp=\"%s\",enabled=\"%c\",addr=\"0x%016"
    // PRIx64
    // "\",func=\"%s\",file=\"%s\",fullname=\"%s/%s\",line=\"%d\",times=\"%d\",original-location=\"%s\"}"
    const CMICmnMIValueResult miValueResult("bkpt", miValueTuple);
    const CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_BreakPointModified, miValueResult);
    bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
  } else {
    // CODETAG_LLDB_STOPPOINT_ID_MAX
    if (sStoppointInfo.m_nMiId > rSessionInfo.m_nBreakpointCntMax) {
      SetErrorDescription(CMIUtilString::Format(
          MIRSRC(IDS_LLDBOUTOFBAND_ERR_STOPPOINT_CNT_EXCEEDED), __func__,
          static_cast<uint64_t>(rSessionInfo.m_nBreakpointCntMax),
          static_cast<uint64_t>(sStoppointInfo.m_nMiId)));
      return MIstatus::failure;
    }
    if (!rSessionInfo.RecordStoppointInfo(sStoppointInfo)) {
      SetErrorDescription(CMIUtilString::Format(
          MIRSRC(IDS_LLDBOUTOFBAND_ERR_STOPPOINT_INFO_SET), __func__,
          static_cast<uint64_t>(sStoppointInfo.m_nMiId)));
      return MIstatus::failure;
    }

    // MI print
    // "=breakpoint-created,bkpt={number=\"%d\",type=\"breakpoint\",disp=\"%s\",enabled=\"%c\",addr=\"0x%016"
    // PRIx64
    // "\",func=\"%s\",file=\"%s\",fullname=\"%s/%s\",line=\"%d\",times=\"%d\",original-location=\"%s\"}"
    const CMICmnMIValueResult miValueResult("bkpt", miValueTuple);
    const CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_BreakPointCreated, miValueResult);
    bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
  }

  return bOk;
}

template bool
CMICmnLLDBDebuggerHandleEvents::HandleEventStoppointAdded(lldb::SBBreakpoint &);
template bool
CMICmnLLDBDebuggerHandleEvents::HandleEventStoppointAdded(lldb::SBWatchpoint &);

template <class T, class>
bool CMICmnLLDBDebuggerHandleEvents::RemoveStoppointInfo(T &vrStoppoint) {
  auto eType = std::is_same<std::remove_cv_t<T>, lldb::SBBreakpoint>::value
                   ? CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint
                   : CMICmnLLDBDebugSessionInfo::eStoppointType_Watchpoint;

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  auto nMiStoppointId =
      rSessionInfo.GetOrCreateMiStoppointId(vrStoppoint.GetID(), eType);
  if (!rSessionInfo.RecordStoppointInfoDelete(nMiStoppointId))
    return MIstatus::failure;

  return rSessionInfo.RemoveLldbToMiStoppointIdMapping(vrStoppoint.GetID(),
                                                       eType);
}

//++
// Details: Handle a LLDB SBThread event.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBThread(
    const lldb::SBEvent &vEvent) {
  if (!ChkForStateChanges())
    return MIstatus::failure;

  bool bOk = MIstatus::success;
  const char *pEventType = "";
  const MIuint nEventType = vEvent.GetType();
  switch (nEventType) {
  case lldb::SBThread::eBroadcastBitStackChanged:
    pEventType = "eBroadcastBitStackChanged";
    bOk = HandleEventSBThreadBitStackChanged(vEvent);
    break;
  case lldb::SBThread::eBroadcastBitThreadSuspended:
    pEventType = "eBroadcastBitThreadSuspended";
    bOk = HandleEventSBThreadSuspended(vEvent);
    break;
  case lldb::SBThread::eBroadcastBitThreadResumed:
    pEventType = "eBroadcastBitThreadResumed";
    break;
  case lldb::SBThread::eBroadcastBitSelectedFrameChanged:
    pEventType = "eBroadcastBitSelectedFrameChanged";
    break;
  case lldb::SBThread::eBroadcastBitThreadSelected:
    pEventType = "eBroadcastBitThreadSelected";
    break;
  default: {
    const CMIUtilString msg(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_UNKNOWN_EVENT),
                              "SBThread", (MIuint)nEventType));
    SetErrorDescription(msg);
    return MIstatus::failure;
  }
  }
  m_pLog->WriteLog(CMIUtilString::Format("##### An SBThread event occurred: %s",
                                         pEventType));

  return bOk;
}

//++
// Details: Handle a LLDB SBThread event.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBThreadSuspended(
    const lldb::SBEvent &vEvent) {
  lldb::SBThread thread = lldb::SBThread::GetThreadFromEvent(vEvent);
  if (!thread.IsValid())
    return MIstatus::success;

  const lldb::StopReason eStopReason = thread.GetStopReason();
  if (eStopReason != lldb::eStopReasonSignal)
    return MIstatus::success;

  // MI print "@thread=%d,signal=%lld"
  const MIuint64 nId = thread.GetStopReasonDataAtIndex(0);
  const CMIUtilString strThread(
      CMIUtilString::Format("%d", thread.GetThreadID()));
  const CMICmnMIValueConst miValueConst(strThread);
  const CMICmnMIValueResult miValueResult("thread", miValueConst);
  CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_Thread, miValueResult);
  const CMIUtilString strSignal(CMIUtilString::Format("%lld", nId));
  const CMICmnMIValueConst miValueConst2(strSignal);
  const CMICmnMIValueResult miValueResult2("signal", miValueConst2);
  miOutOfBandRecord.Add(miValueResult2);
  return MiOutOfBandRecordToStdout(miOutOfBandRecord);
}

//++
// Details: Handle a LLDB SBThread event.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBThreadBitStackChanged(
    const lldb::SBEvent &vEvent) {
  lldb::SBThread thread = lldb::SBThread::GetThreadFromEvent(vEvent);
  if (!thread.IsValid())
    return MIstatus::success;

  lldb::SBStream streamOut;
  const bool bOk = thread.GetStatus(streamOut);
  return bOk && TextToStdout(streamOut.GetData());
}

//++
// Details: Handle a LLDB SBTarget event.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBTarget(
    const lldb::SBEvent &vEvent) {
  if (!ChkForStateChanges())
    return MIstatus::failure;

  bool bOk = MIstatus::success;
  const char *pEventType = "";
  const MIuint nEventType = vEvent.GetType();
  switch (nEventType) {
  case lldb::SBTarget::eBroadcastBitBreakpointChanged:
    pEventType = "eBroadcastBitBreakpointChanged";
    break;
  case lldb::SBTarget::eBroadcastBitModulesLoaded:
    pEventType = "eBroadcastBitModulesLoaded";
    bOk = HandleTargetEventBroadcastBitModulesLoaded(vEvent);
    break;
  case lldb::SBTarget::eBroadcastBitModulesUnloaded:
    pEventType = "eBroadcastBitModulesUnloaded";
    bOk = HandleTargetEventBroadcastBitModulesUnloaded(vEvent);
    break;
  case lldb::SBTarget::eBroadcastBitWatchpointChanged:
    pEventType = "eBroadcastBitWatchpointChanged";
    break;
  case lldb::SBTarget::eBroadcastBitSymbolsLoaded:
    pEventType = "eBroadcastBitSymbolsLoaded";
    break;
  default: {
    const CMIUtilString msg(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_UNKNOWN_EVENT),
                              "SBTarget", (MIuint)nEventType));
    SetErrorDescription(msg);
    return MIstatus::failure;
  }
  }
  m_pLog->WriteLog(CMIUtilString::Format("##### An SBTarget event occurred: %s",
                                         pEventType));

  return bOk;
}

//++
// Details: Print to stdout
// "=library-loaded,id=\"%s\",target-name=\"%s\",host-name=\"%s\",symbols-loaded="%d"[,symbols-path=\"%s\"],loaded_addr=\"0x%016"
// PRIx64"\""
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Function succeeded.
//          MIstatus::failure - Function failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleTargetEventBroadcastBitModulesLoaded(
    const lldb::SBEvent &vEvent) {
  bool bOk = MIstatus::failure;
  const MIuint nSize = lldb::SBTarget::GetNumModulesFromEvent(vEvent);
  for (MIuint nIndex = 0; nIndex < nSize; ++nIndex) {
    const lldb::SBModule sbModule =
        lldb::SBTarget::GetModuleAtIndexFromEvent(nIndex, vEvent);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_TargetModuleLoaded);
    const bool bWithExtraFields = true;
    bOk = MiHelpGetModuleInfo(sbModule, bWithExtraFields, miOutOfBandRecord);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord);
    if (!bOk)
      break;
  }

  return bOk;
}

//++
// Details: Print to stdout
// "=library-unloaded,id=\"%s\",target-name=\"%s\",host-name=\"%s\",symbols-loaded="%d"[,symbols-path=\"%s\"],loaded_addr=\"0x%016"
// PRIx64"\""
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Function succeeded.
//          MIstatus::failure - Function failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::
    HandleTargetEventBroadcastBitModulesUnloaded(const lldb::SBEvent &vEvent) {
  bool bOk = MIstatus::failure;
  const MIuint nSize = lldb::SBTarget::GetNumModulesFromEvent(vEvent);
  for (MIuint nIndex = 0; nIndex < nSize; ++nIndex) {
    const lldb::SBModule sbModule =
        lldb::SBTarget::GetModuleAtIndexFromEvent(nIndex, vEvent);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_TargetModuleUnloaded);
    const bool bWithExtraFields = false;
    bOk = MiHelpGetModuleInfo(sbModule, bWithExtraFields, miOutOfBandRecord);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord);
    if (!bOk)
      break;
  }

  return bOk;
}

//++
// Details: Build module information for =library-loaded/=library-unloaded:
// "id=\"%s\",target-name=\"%s\",host-name=\"%s\",symbols-loaded="%d"[,symbols-path=\"%s\"],loaded_addr=\"0x%016"
// PRIx64"\""
// Type:    Method.
// Args:    vwrMiValueList    - (W) MI value list object.
// Return:  MIstatus::success - Function succeeded.
//          MIstatus::failure - Function failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::MiHelpGetModuleInfo(
    const lldb::SBModule &vModule, const bool vbWithExtraFields,
    CMICmnMIOutOfBandRecord &vwrMiOutOfBandRecord) {
  bool bOk = MIstatus::success;

  // First, build standard fields:
  // Build "id" field
  std::unique_ptr<char[]> apPath(new char[PATH_MAX]);
  vModule.GetFileSpec().GetPath(apPath.get(), PATH_MAX);
  const CMIUtilString strTargetPath(apPath.get());
  const CMICmnMIValueConst miValueConst(strTargetPath.AddSlashes());
  const CMICmnMIValueResult miValueResult("id", miValueConst);
  vwrMiOutOfBandRecord.Add(miValueResult);
  // Build "target-name" field
  const CMICmnMIValueConst miValueConst2(strTargetPath.AddSlashes());
  const CMICmnMIValueResult miValueResult2("target-name", miValueConst2);
  vwrMiOutOfBandRecord.Add(miValueResult2);
  // Build "host-name" field
  vModule.GetPlatformFileSpec().GetPath(apPath.get(), PATH_MAX);
  const CMIUtilString strHostPath(apPath.get());
  const CMICmnMIValueConst miValueConst3(strHostPath.AddSlashes());
  const CMICmnMIValueResult miValueResult3("host-name", miValueConst3);
  vwrMiOutOfBandRecord.Add(miValueResult3);

  // Then build extra fields if needed:
  if (vbWithExtraFields) {
    // Build "symbols-loaded" field
    vModule.GetSymbolFileSpec().GetPath(apPath.get(), PATH_MAX);
    const CMIUtilString strSymbolsPath(apPath.get());
    const bool bSymbolsLoaded =
        !CMIUtilString::Compare(strHostPath, strSymbolsPath);
    const CMICmnMIValueConst miValueConst4(
        CMIUtilString::Format("%d", bSymbolsLoaded));
    const CMICmnMIValueResult miValueResult4("symbols-loaded", miValueConst4);
    vwrMiOutOfBandRecord.Add(miValueResult4);
    // Build "symbols-path" field
    if (bSymbolsLoaded) {
      const CMICmnMIValueConst miValueConst5(strSymbolsPath.AddSlashes());
      const CMICmnMIValueResult miValueResult5("symbols-path", miValueConst5);
      vwrMiOutOfBandRecord.Add(miValueResult5);
    }
    // Build "loaded_addr" field
    lldb::SBAddress sbAddress(vModule.GetObjectFileHeaderAddress());
    CMICmnLLDBDebugSessionInfo &rSessionInfo(
        CMICmnLLDBDebugSessionInfo::Instance());
    const lldb::addr_t nLoadAddress(
        sbAddress.GetLoadAddress(rSessionInfo.GetTarget()));
    const CMIUtilString strLoadedAddr(
        nLoadAddress != LLDB_INVALID_ADDRESS
            ? CMIUtilString::Format("0x%016" PRIx64, nLoadAddress)
            : "-");
    const CMICmnMIValueConst miValueConst6(strLoadedAddr);
    const CMICmnMIValueResult miValueResult6("loaded_addr", miValueConst6);
    vwrMiOutOfBandRecord.Add(miValueResult6);

    // Build "size" field
    lldb::SBSection sbSection = sbAddress.GetSection();
    const CMIUtilString strSize(
        CMIUtilString::Format("%" PRIu64, sbSection.GetByteSize()));
    const CMICmnMIValueConst miValueConst7(strSize);
    const CMICmnMIValueResult miValueResult7("size", miValueConst7);
    vwrMiOutOfBandRecord.Add(miValueResult7);
  }

  return bOk;
}

//++
// Details: Handle a LLDB SBCommandInterpreter event.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB command interpreter event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleEventSBCommandInterpreter(
    const lldb::SBEvent &vEvent) {
  // This function is not used
  // *** This function is under development

  const char *pEventType = "";
  const MIuint nEventType = vEvent.GetType();
  switch (nEventType) {
  case lldb::SBCommandInterpreter::eBroadcastBitThreadShouldExit:
    pEventType = "eBroadcastBitThreadShouldExit";
    // ToDo: IOR: Reminder to maybe handle this here
    // const MIuint nEventType = event.GetType();
    // if (nEventType &
    // lldb::SBCommandInterpreter::eBroadcastBitThreadShouldExit)
    //{
    //  m_pClientDriver->SetExitApplicationFlag();
    //  vrbYesExit = true;
    //  return MIstatus::success;
    //}
    break;
  case lldb::SBCommandInterpreter::eBroadcastBitResetPrompt:
    pEventType = "eBroadcastBitResetPrompt";
    break;
  case lldb::SBCommandInterpreter::eBroadcastBitQuitCommandReceived: {
    pEventType = "eBroadcastBitQuitCommandReceived";
    const bool bForceExit = true;
    CMICmnLLDBDebugger::Instance().GetDriver().SetExitApplicationFlag(
        bForceExit);
    break;
  }
  case lldb::SBCommandInterpreter::eBroadcastBitAsynchronousOutputData:
    pEventType = "eBroadcastBitAsynchronousOutputData";
    break;
  case lldb::SBCommandInterpreter::eBroadcastBitAsynchronousErrorData:
    pEventType = "eBroadcastBitAsynchronousErrorData";
    break;
  default: {
    const CMIUtilString msg(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_UNKNOWN_EVENT),
                              "SBCommandInterpreter", (MIuint)nEventType));
    SetErrorDescription(msg);
    return MIstatus::failure;
  }
  }
  m_pLog->WriteLog(CMIUtilString::Format(
      "##### An SBCommandInterpreter event occurred: %s", pEventType));

  return MIstatus::success;
}

//++
// Details: Handle SBProcess event eBroadcastBitStateChanged.
// Type:    Method.
// Args:    vEvent          - (R) An LLDB event object.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventBroadcastBitStateChanged(
    const lldb::SBEvent &vEvent) {
  // Make sure the program hasn't been auto-restarted:
  if (lldb::SBProcess::GetRestartedFromEvent(vEvent))
    return MIstatus::success;

  // Something changed in the process; get the event and report the process's
  // current
  // status and location
  const lldb::StateType eEventState =
      lldb::SBProcess::GetStateFromEvent(vEvent);
  if (eEventState == lldb::eStateInvalid)
    return MIstatus::success;

  bool bOk = true;

  // When the process is running (or in stepping mode) any invocation of
  // SBThread::IsValid() will return false. Moreover, the thread list
  // cannot be modified when the state changes from `stopped` to `running`
  // so it is not necessary to check changes of threads state in this case.
  if (eEventState != lldb::eStateStepping &&
      eEventState != lldb::eStateRunning) {
    bool bOk = ChkForStateChanges();
    bOk = bOk && GetProcessStdout();
    bOk = bOk && GetProcessStderr();
    if (!bOk)
      return MIstatus::failure;
  }

  lldb::SBProcess process = lldb::SBProcess::GetProcessFromEvent(vEvent);
  if (!process.IsValid()) {
    const CMIUtilString msg(CMIUtilString::Format(
        MIRSRC(IDS_LLDBOUTOFBAND_ERR_PROCESS_INVALID), "SBProcess",
        "HandleProcessEventBroadcastBitStateChanged()"));
    SetErrorDescription(msg);
    return MIstatus::failure;
  }

  bool bShouldBrk = true;
  const char *pEventType = "";
  switch (eEventState) {
  case lldb::eStateUnloaded:
    pEventType = "eStateUnloaded";
    break;
  case lldb::eStateConnected:
    pEventType = "eStateConnected";
    break;
  case lldb::eStateAttaching:
    pEventType = "eStateAttaching";
    break;
  case lldb::eStateLaunching:
    pEventType = "eStateLaunching";
    break;
  case lldb::eStateStopped:
    pEventType = "eStateStopped";
    bOk = HandleProcessEventStateStopped(vEvent, bShouldBrk);
    if (bShouldBrk)
      break;
    LLVM_FALLTHROUGH;
  case lldb::eStateCrashed:
  case lldb::eStateSuspended:
    pEventType = "eStateSuspended";
    bOk = HandleProcessEventStateSuspended(vEvent);
    break;
  case lldb::eStateRunning:
    pEventType = "eStateRunning";
    bOk = HandleProcessEventStateRunning();
    break;
  case lldb::eStateStepping:
    pEventType = "eStateStepping";
    bOk = HandleProcessEventStateRunning();
    break;
  case lldb::eStateDetached:
    pEventType = "eStateDetached";
    break;
  case lldb::eStateExited:
    // Don't exit from lldb-mi here. We should be able to re-run target.
    pEventType = "eStateExited";
    bOk = HandleProcessEventStateExited();
    break;
  default: {
    const CMIUtilString msg(CMIUtilString::Format(
        MIRSRC(IDS_LLDBOUTOFBAND_ERR_UNKNOWN_EVENT),
        "SBProcess BroadcastBitStateChanged", (MIuint)eEventState));
    SetErrorDescription(msg);
    return MIstatus::failure;
  }
  }

  // ToDo: Remove when finished coding application
  m_pLog->WriteLog(CMIUtilString::Format(
      "##### An SB Process event BroadcastBitStateChanged occurred: %s",
      pEventType));

  return bOk;
}

//++
// Details: Asynchronous event handler for LLDB Process state suspended.
// Type:    Method.
// Args:    vEvent  - (R) An LLDB event object.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStateSuspended(
    const lldb::SBEvent &vEvent) {
  bool bOk = MIstatus::success;
  lldb::SBStream streamOut;
  lldb::SBDebugger &rDebugger =
      CMICmnLLDBDebugSessionInfo::Instance().GetDebugger();
  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  lldb::SBTarget target = sbProcess.GetTarget();
  if (rDebugger.GetSelectedTarget() == target) {
    if (!UpdateSelectedThread())
      return MIstatus::failure;
    sbProcess.GetDescription(streamOut);
    // Add a delimiter between process' and threads' info.
    streamOut.Printf("\n");
    for (uint32_t i = 0, e = sbProcess.GetNumThreads(); i < e; ++i) {
      const lldb::SBThread thread = sbProcess.GetThreadAtIndex(i);
      if (!thread.IsValid())
        continue;
      thread.GetDescription(streamOut);
    }
    bOk = TextToStdout(streamOut.GetData());
  } else {
    const MIuint nTargetIndex = rDebugger.GetIndexOfTarget(target);
    if (nTargetIndex != UINT_MAX)
      streamOut.Printf("Target %" PRIu64 ": (", (uint64_t)nTargetIndex);
    else
      streamOut.Printf("Target <unknown index>: (");
    target.GetDescription(streamOut, lldb::eDescriptionLevelBrief);
    streamOut.Printf(") stopped.\n");
    bOk = TextToStdout(streamOut.GetData());
  }

  return bOk;
}

//++
// Details: Print to stdout MI formatted text to indicate process stopped.
// Type:    Method.
// Args:    vwrbShouldBrk   - (W) True = Yes break, false = do not.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStateStopped(
    const lldb::SBEvent &vrEvent, bool &vwrbShouldBrk) {
  if (!UpdateSelectedThread())
    return MIstatus::failure;

  const char *pEventType = "";
  bool bOk = MIstatus::success;
  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  const lldb::StopReason eStoppedReason =
      sbProcess.GetSelectedThread().GetStopReason();
  switch (eStoppedReason) {
  case lldb::eStopReasonInvalid:
    pEventType = "eStopReasonInvalid";
    vwrbShouldBrk = false;
    break;
  case lldb::eStopReasonNone:
    pEventType = "eStopReasonNone";
    break;
  case lldb::eStopReasonTrace:
    pEventType = "eStopReasonTrace";
    bOk = HandleProcessEventStopReasonTrace();
    break;
  case lldb::eStopReasonBreakpoint:
    pEventType = "eStopReasonBreakpoint";
    bOk = HandleProcessEventStopReasonBreakpoint();
    break;
  case lldb::eStopReasonWatchpoint:
    pEventType = "eStopReasonWatchpoint";
    bOk = HandleProcessEventStopReasonWatchpoint();
    break;
  case lldb::eStopReasonSignal:
    pEventType = "eStopReasonSignal";
    bOk = HandleProcessEventStopSignal(vrEvent);
    break;
  case lldb::eStopReasonException:
    pEventType = "eStopReasonException";
    bOk = HandleProcessEventStopException();
    break;
  case lldb::eStopReasonExec:
    pEventType = "eStopReasonExec";
    break;
  case lldb::eStopReasonPlanComplete:
    pEventType = "eStopReasonPlanComplete";
    bOk = HandleProcessEventStopReasonTrace();
    break;
  case lldb::eStopReasonThreadExiting:
    pEventType = "eStopReasonThreadExiting";
    break;
  case lldb::eStopReasonInstrumentation:
    pEventType = "eStopReasonInstrumentation";
    break;
  case lldb::eStopReasonProcessorTrace:
    pEventType = "eStopReasonProcessorTrace";
    break;
  case lldb::eStopReasonFork:
    pEventType = "eStopReasonFork";
    break;
  case lldb::eStopReasonVFork:
    pEventType = "eStopReasonVFork";
    break;
  case lldb::eStopReasonVForkDone:
    pEventType = "eStopReasonVForkDone";
    break;
  }

  // ToDo: Remove when finished coding application
  m_pLog->WriteLog(CMIUtilString::Format(
      "##### An SB Process event stop state occurred: %s", pEventType));

  return bOk;
}

//++
// Details: Asynchronous event handler for LLDB Process stop signal.
// Type:    Method.
// Args:    vrEvent           - (R) An LLDB broadcast event.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStopSignal(
    const lldb::SBEvent &vrEvent) {
  bool bOk = MIstatus::success;

  InitializeSignals();
  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  const MIuint64 nStopReason =
      sbProcess.GetSelectedThread().GetStopReasonDataAtIndex(0);
  const bool bInterrupted = lldb::SBProcess::GetInterruptedFromEvent(vrEvent);
  if (nStopReason == m_SIGINT || (nStopReason == m_SIGSTOP && bInterrupted)) {
    // MI print
    // "*stopped,reason=\"signal-received\",signal-name=\"SIGINT\",signal-meaning=\"Interrupt\",frame={%s},thread-id=\"%d\",stopped-threads=\"all\""
    const CMICmnMIValueConst miValueConst("signal-received");
    const CMICmnMIValueResult miValueResult("reason", miValueConst);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
    const CMICmnMIValueConst miValueConst2("SIGINT");
    const CMICmnMIValueResult miValueResult2("signal-name", miValueConst2);
    miOutOfBandRecord.Add(miValueResult2);
    const CMICmnMIValueConst miValueConst3("Interrupt");
    const CMICmnMIValueResult miValueResult3("signal-meaning", miValueConst3);
    miOutOfBandRecord.Add(miValueResult3);
    CMICmnMIValueTuple miValueTuple;
    bOk = bOk && MiHelpGetCurrentThreadFrame(miValueTuple);
    const CMICmnMIValueResult miValueResult4("frame", miValueTuple);
    miOutOfBandRecord.Add(miValueResult4);
    const CMIUtilString strThreadId(CMIUtilString::Format(
        "%" PRIu32, sbProcess.GetSelectedThread().GetIndexID()));
    const CMICmnMIValueConst miValueConst5(strThreadId);
    const CMICmnMIValueResult miValueResult5("thread-id", miValueConst5);
    miOutOfBandRecord.Add(miValueResult5);
    const CMICmnMIValueConst miValueConst6("all");
    const CMICmnMIValueResult miValueResult6("stopped-threads", miValueConst6);
    miOutOfBandRecord.Add(miValueResult6);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
  } else if (nStopReason == m_SIGSTOP) {
    // MI print
    // "*stopped,reason=\"signal-received\",signal-name=\"SIGSTOP\",signal-meaning=\"Stop\",frame={%s},thread-id=\"%d\",stopped-threads=\"all\""
    const CMICmnMIValueConst miValueConst("signal-received");
    const CMICmnMIValueResult miValueResult("reason", miValueConst);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
    const CMICmnMIValueConst miValueConst2("SIGSTOP");
    const CMICmnMIValueResult miValueResult2("signal-name", miValueConst2);
    miOutOfBandRecord.Add(miValueResult2);
    const CMICmnMIValueConst miValueConst3("Stop");
    const CMICmnMIValueResult miValueResult3("signal-meaning", miValueConst3);
    miOutOfBandRecord.Add(miValueResult3);
    CMICmnMIValueTuple miValueTuple;
    bOk = bOk && MiHelpGetCurrentThreadFrame(miValueTuple);
    const CMICmnMIValueResult miValueResult4("frame", miValueTuple);
    miOutOfBandRecord.Add(miValueResult4);
    const CMIUtilString strThreadId(CMIUtilString::Format(
        "%" PRIu32, sbProcess.GetSelectedThread().GetIndexID()));
    const CMICmnMIValueConst miValueConst5(strThreadId);
    const CMICmnMIValueResult miValueResult5("thread-id", miValueConst5);
    miOutOfBandRecord.Add(miValueResult5);
    const CMICmnMIValueConst miValueConst6("all");
    const CMICmnMIValueResult miValueResult6("stopped-threads", miValueConst6);
    miOutOfBandRecord.Add(miValueResult6);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
  } else if (nStopReason == m_SIGSEGV) {
    // MI print
    // "*stopped,reason=\"signal-received\",signal-name=\"SIGSEGV\",signal-meaning=\"Segmentation
    // fault\",thread-id=\"%d\",frame={%s}"
    const CMICmnMIValueConst miValueConst("signal-received");
    const CMICmnMIValueResult miValueResult("reason", miValueConst);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
    const CMICmnMIValueConst miValueConst2("SIGSEGV");
    const CMICmnMIValueResult miValueResult2("signal-name", miValueConst2);
    miOutOfBandRecord.Add(miValueResult2);
    const CMICmnMIValueConst miValueConst3("Segmentation fault");
    const CMICmnMIValueResult miValueResult3("signal-meaning", miValueConst3);
    miOutOfBandRecord.Add(miValueResult3);
    CMICmnMIValueTuple miValueTuple;
    bOk = bOk && MiHelpGetCurrentThreadFrame(miValueTuple);
    const CMICmnMIValueResult miValueResult4("frame", miValueTuple);
    miOutOfBandRecord.Add(miValueResult4);
    const CMIUtilString strThreadId(CMIUtilString::Format(
        "%d", sbProcess.GetSelectedThread().GetIndexID()));
    const CMICmnMIValueConst miValueConst5(strThreadId);
    const CMICmnMIValueResult miValueResult5("thread-id", miValueConst5);
    miOutOfBandRecord.Add(miValueResult5);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord);
    // Note no "(gdb)" output here
  } else if (nStopReason == m_SIGTRAP) {
    lldb::SBThread thread = sbProcess.GetSelectedThread();
    const MIuint nFrames = thread.GetNumFrames();
    if (nFrames > 0) {
      lldb::SBFrame frame = thread.GetFrameAtIndex(0);
      const char *pFnName = frame.GetFunctionName();
      if (pFnName != nullptr) {
        const CMIUtilString fnName = CMIUtilString(pFnName);
        static const CMIUtilString threadCloneFn =
            CMIUtilString("__pthread_clone");

        if (CMIUtilString::Compare(threadCloneFn, fnName)) {
          if (sbProcess.IsValid())
            sbProcess.Continue();
        }
      }
    }
  } else {
    // MI print
    // "*stopped,reason=\"signal-received\",signal-name=\"%s\",thread-id=\"%d\",stopped-threads=\"all\""
    // MI print
    // "*stopped,reason=\"signal-received\",signal=\"%d\",thread-id=\"%d\",stopped-threads=\"all\""
    const CMICmnMIValueConst miValueConst("signal-received");
    const CMICmnMIValueResult miValueResult("reason", miValueConst);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
    lldb::SBUnixSignals sbUnixSignals = sbProcess.GetUnixSignals();
    const char *pSignal = sbUnixSignals.GetSignalAsCString(nStopReason);
    if (pSignal) {
      const CMICmnMIValueConst miValueConst2(pSignal);
      const CMICmnMIValueResult miValueResult2("signal-name", miValueConst2);
      miOutOfBandRecord.Add(miValueResult2);
    } else {
      const CMIUtilString strSignal(
          CMIUtilString::Format("%" PRIu64, nStopReason));
      const CMICmnMIValueConst miValueConst2(strSignal);
      const CMICmnMIValueResult miValueResult2("signal", miValueConst2);
      miOutOfBandRecord.Add(miValueResult2);
    }
    const CMIUtilString strThreadId(CMIUtilString::Format(
        "%d", sbProcess.GetSelectedThread().GetIndexID()));
    const CMICmnMIValueConst miValueConst3(strThreadId);
    const CMICmnMIValueResult miValueResult3("thread-id", miValueConst3);
    miOutOfBandRecord.Add(miValueResult3);
    const CMICmnMIValueConst miValueConst4("all");
    const CMICmnMIValueResult miValueResult4("stopped-threads", miValueConst4);
    miOutOfBandRecord.Add(miValueResult4);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
  }
  return bOk;
}

//++
// Details: Asynchronous event handler for LLDB Process stop exception.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStopException() {
  const lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  lldb::SBThread sbThread = sbProcess.GetSelectedThread();
  const size_t nStopDescriptionLen = sbThread.GetStopDescription(nullptr, 0);
  std::unique_ptr<char[]> apStopDescription(new char[nStopDescriptionLen]);
  sbThread.GetStopDescription(apStopDescription.get(), nStopDescriptionLen);

  // MI print
  // "*stopped,reason=\"exception-received\",exception=\"%s\",thread-id=\"%d\",stopped-threads=\"all\""
  const CMICmnMIValueConst miValueConst("exception-received");
  const CMICmnMIValueResult miValueResult("reason", miValueConst);
  CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
  const CMIUtilString strReason(apStopDescription.get());
  const CMICmnMIValueConst miValueConst2(strReason);
  const CMICmnMIValueResult miValueResult2("exception", miValueConst2);
  miOutOfBandRecord.Add(miValueResult2);
  const CMIUtilString strThreadId(
      CMIUtilString::Format("%d", sbThread.GetIndexID()));
  const CMICmnMIValueConst miValueConst3(strThreadId);
  const CMICmnMIValueResult miValueResult3("thread-id", miValueConst3);
  miOutOfBandRecord.Add(miValueResult3);
  const CMICmnMIValueConst miValueConst4("all");
  const CMICmnMIValueResult miValueResult4("stopped-threads", miValueConst4);
  miOutOfBandRecord.Add(miValueResult4);
  bool bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
  bOk = bOk && CMICmnStreamStdout::WritePrompt();

  return bOk;
}

//++
// Details: Form partial MI response in a MI value tuple object.
// Type:    Method.
// Args:    vwrMiValueTuple   - (W) MI value tuple object.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::MiHelpGetCurrentThreadFrame(
    CMICmnMIValueTuple &vwrMiValueTuple) {
  CMIUtilString strThreadFrame;
  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  lldb::SBThread thread = sbProcess.GetSelectedThread();
  const MIuint nFrame = thread.GetNumFrames();
  if (nFrame == 0) {
    // MI print
    // "addr=\"??\",func=\"??\",file=\"??\",fullname=\"??\",line=\"??\""
    const CMICmnMIValueConst miValueConst("??");
    const CMICmnMIValueResult miValueResult("addr", miValueConst);
    CMICmnMIValueTuple miValueTuple(miValueResult);
    const CMICmnMIValueResult miValueResult2("func", miValueConst);
    miValueTuple.Add(miValueResult2);
    const CMICmnMIValueResult miValueResult4("file", miValueConst);
    miValueTuple.Add(miValueResult4);
    const CMICmnMIValueResult miValueResult5("fullname", miValueConst);
    miValueTuple.Add(miValueResult5);
    const CMICmnMIValueResult miValueResult6("line", miValueConst);
    miValueTuple.Add(miValueResult6);

    vwrMiValueTuple = miValueTuple;

    return MIstatus::success;
  }

  CMICmnMIValueTuple miValueTuple;
  if (!CMICmnLLDBDebugSessionInfo::Instance().MIResponseFormFrameInfo(
          thread, 0, CMICmnLLDBDebugSessionInfo::eFrameInfoFormat_NoArguments,
          miValueTuple)) {
    SetErrorDescription(CMIUtilString::Format(
        MIRSRC(IDS_LLDBOUTOFBAND_ERR_FORM_MI_RESPONSE), __func__));
    return MIstatus::failure;
  }

  vwrMiValueTuple = miValueTuple;

  return MIstatus::success;
}

//++
// Details: Asynchronous event handler for LLDB Process stop reason breakpoint.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStopReasonBreakpoint() {
  // CODETAG_DEBUG_SESSION_RUNNING_PROG_RECEIVED_SIGINT_PAUSE_PROGRAM
  if (!CMIDriver::Instance().SetDriverStateRunningNotDebugging()) {
    const CMIUtilString &rErrMsg(CMIDriver::Instance().GetErrorDescription());
    SetErrorDescription(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_SETNEWDRIVERSTATE),
                              __func__, rErrMsg.c_str()));
    return MIstatus::failure;
  }

  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  const MIuint64 breakpointId =
      sbProcess.GetSelectedThread().GetStopReasonDataAtIndex(0);
  lldb::SBBreakpoint breakpoint =
      CMICmnLLDBDebugSessionInfo::Instance().GetTarget().FindBreakpointByID(
          (MIuint)breakpointId);

  return MiStoppedAtBreakPoint(breakpointId, breakpoint);
}

//++
// Details: Asynchronous event handler for LLDB Process stop reason watchpoint.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStopReasonWatchpoint() {
  // CODETAG_DEBUG_SESSION_RUNNING_PROG_RECEIVED_SIGINT_PAUSE_PROGRAM
  if (!CMIDriver::Instance().SetDriverStateRunningNotDebugging()) {
    const CMIUtilString &rErrMsg(CMIDriver::Instance().GetErrorDescription());
    SetErrorDescription(
        CMIUtilString::Format(MIRSRC(IDS_LLDBOUTOFBAND_ERR_SETNEWDRIVERSTATE),
                              __func__, rErrMsg.c_str()));
    return MIstatus::failure;
  }

  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  const auto watchpointId = static_cast<uint32_t>(
      sbProcess.GetSelectedThread().GetStopReasonDataAtIndex(0U));
  lldb::SBWatchpoint watchpoint =
      CMICmnLLDBDebugSessionInfo::Instance().GetTarget().FindWatchpointByID(
          watchpointId);

  return MiStoppedAtWatchpoint(watchpoint);
}

//++
// Details: Form the MI Out-of-band response for stopped reason on hitting a
// break point.
// Type:    Method.
// Args:    vBreakpointId    - (R) The LLDB break point's ID
//          vBreakpoint      - (R) THe LLDB break point object.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::MiStoppedAtBreakPoint(
    const MIuint64 vBreakpointId, const lldb::SBBreakpoint &vBreakpoint) {
  bool bOk = MIstatus::success;

  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  lldb::SBThread thread = sbProcess.GetSelectedThread();
  const MIuint nFrame = thread.GetNumFrames();
  if (nFrame == 0) {
    // MI print
    // "*stopped,reason=\"breakpoint-hit\",disp=\"del\",bkptno=\"%d\",frame={},thread-id=\"%d\",stopped-threads=\"all\""
    const CMICmnMIValueConst miValueConst("breakpoint-hit");
    const CMICmnMIValueResult miValueResult("reason", miValueConst);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
    const CMICmnMIValueConst miValueConst2("del");
    const CMICmnMIValueResult miValueResult2("disp", miValueConst2);
    miOutOfBandRecord.Add(miValueResult2);
    const CMIUtilString strBkp(CMIUtilString::Format("%d", vBreakpointId));
    const CMICmnMIValueConst miValueConst3(strBkp);
    CMICmnMIValueResult miValueResult3("bkptno", miValueConst3);
    miOutOfBandRecord.Add(miValueResult3);
    const CMICmnMIValueConst miValueConst4("{}");
    const CMICmnMIValueResult miValueResult4("frame", miValueConst4);
    miOutOfBandRecord.Add(miValueResult4);
    const CMIUtilString strThreadId(
        CMIUtilString::Format("%d", vBreakpoint.GetThreadIndex()));
    const CMICmnMIValueConst miValueConst5(strThreadId);
    const CMICmnMIValueResult miValueResult5("thread-id", miValueConst5);
    miOutOfBandRecord.Add(miValueResult5);
    const CMICmnMIValueConst miValueConst6("all");
    const CMICmnMIValueResult miValueResult6("stopped-threads", miValueConst6);
    miOutOfBandRecord.Add(miValueResult6);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
    return bOk;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  // MI print
  // "*stopped,reason=\"breakpoint-hit\",disp=\"del\",bkptno=\"%d\",frame={addr=\"0x%016"
  // PRIx64
  // "\",func=\"%s\",args=[],file=\"%s\",fullname=\"%s\",line=\"%d\"},thread-id=\"%d\",stopped-threads=\"all\""
  const CMICmnMIValueConst miValueConst("breakpoint-hit");
  const CMICmnMIValueResult miValueResult("reason", miValueConst);
  CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
  const CMICmnMIValueConst miValueConstA("del");
  const CMICmnMIValueResult miValueResultA("disp", miValueConstA);
  miOutOfBandRecord.Add(miValueResultA);
  const CMIUtilString strBkp(CMIUtilString::Format("%d", vBreakpointId));
  const CMICmnMIValueConst miValueConstB(strBkp);
  CMICmnMIValueResult miValueResultB("bkptno", miValueConstB);
  miOutOfBandRecord.Add(miValueResultB);

  // frame={addr=\"0x%016" PRIx64
  // "\",func=\"%s\",args=[],file=\"%s\",fullname=\"%s\",line=\"%d\"}
  if (bOk) {
    CMICmnMIValueTuple miValueTuple;
    bOk = bOk && rSessionInfo.MIResponseFormFrameInfo(
                     thread, 0,
                     CMICmnLLDBDebugSessionInfo::eFrameInfoFormat_AllArguments,
                     miValueTuple);
    const CMICmnMIValueResult miValueResult8("frame", miValueTuple);
    miOutOfBandRecord.Add(miValueResult8);
  }

  // Add to MI thread-id=\"%d\",stopped-threads=\"all\"
  if (bOk) {
    const CMIUtilString strThreadId(
        CMIUtilString::Format("%d", thread.GetIndexID()));
    const CMICmnMIValueConst miValueConst8(strThreadId);
    const CMICmnMIValueResult miValueResult8("thread-id", miValueConst8);
    miOutOfBandRecord.Add(miValueResult8);
  }
  if (bOk) {
    const CMICmnMIValueConst miValueConst9("all");
    const CMICmnMIValueResult miValueResult9("stopped-threads", miValueConst9);
    miOutOfBandRecord.Add(miValueResult9);
    bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
  }

  return MIstatus::success;
}

//++
// Details: Form the MI Out-of-band response for stopped reason on hitting a
//          watch point.
// Type:    Method.
// Args:    vrWatchpointId    - (R) The LLDB watch point.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::MiStoppedAtWatchpoint(
    lldb::SBWatchpoint &vrWatchpoint) {
  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  auto nGdbBreakpointId = rSessionInfo.GetOrCreateMiStoppointId(
      vrWatchpoint.GetID(),
      CMICmnLLDBDebugSessionInfo::eStoppointType_Watchpoint);

  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(nGdbBreakpointId, sStoppointInfo))
    return MIstatus::failure;

  const char *reason = sStoppointInfo.m_watchpointRead
                           ? sStoppointInfo.m_watchpointWrite
                                 ? "access-watchpoint-trigger"
                                 : "read-watchpoint-trigger"
                           : "watchpoint-trigger";

  const CMICmnMIValueConst miValueConstReason(reason);
  const CMICmnMIValueResult miValueResultReason("reason", miValueConstReason);
  CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResultReason);

  // value={...} is not supported yet because it is not obvious how to get the
  // old value.

  // wpt={...}
  CMICmnMIValueResult miValueResultWpt;
  rSessionInfo.MIResponseFormWatchpointInfo(sStoppointInfo, miValueResultWpt);
  miOutOfBandRecord.Add(miValueResultWpt);

  // frame={...}
  lldb::SBThread thread = rSessionInfo.GetProcess().GetSelectedThread();
  if (thread.GetNumFrames() > 0) {
    CMICmnMIValueTuple miValueTupleFrame;
    if (!rSessionInfo.MIResponseFormFrameInfo(
            thread, 0,
            CMICmnLLDBDebugSessionInfo::eFrameInfoFormat_AllArguments,
            miValueTupleFrame))
      return MIstatus::failure;

    const CMICmnMIValueResult miValueResultFrame("frame", miValueTupleFrame);
    miOutOfBandRecord.Add(miValueResultFrame);
  }

  // thread-id=...
  const CMIUtilString strThreadId(
      CMIUtilString::Format("%d", thread.GetIndexID()));
  const CMICmnMIValueConst miValueConstThread(strThreadId);
  const CMICmnMIValueResult miValueResultThread("thread-id",
                                                miValueConstThread);
  miOutOfBandRecord.Add(miValueResultThread);

  // stopped-threads=...
  const CMICmnMIValueConst miValueConstStopped("all");
  const CMICmnMIValueResult miValueResultStopped("stopped-threads",
                                                 miValueConstStopped);
  miOutOfBandRecord.Add(miValueResultStopped);

  if (!MiOutOfBandRecordToStdout(miOutOfBandRecord))
    return MIstatus::failure;

  return CMICmnStreamStdout::WritePrompt();
}

//++
// Details: Asynchronous event handler for LLDB Process stop reason trace.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStopReasonTrace() {
  bool bOk = true;
  lldb::SBProcess sbProcess =
      CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
  lldb::SBThread thread = sbProcess.GetSelectedThread();
  const MIuint nFrame = thread.GetNumFrames();
  if (nFrame == 0) {
    // MI print "*stopped,reason=\"trace\",stopped-threads=\"all\""
    const CMICmnMIValueConst miValueConst("trace");
    const CMICmnMIValueResult miValueResult("reason", miValueConst);
    CMICmnMIOutOfBandRecord miOutOfBandRecord(
        CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
    const CMICmnMIValueConst miValueConst2("all");
    const CMICmnMIValueResult miValueResult2("stopped-threads", miValueConst2);
    miOutOfBandRecord.Add(miValueResult2);
    bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
    bOk = bOk && CMICmnStreamStdout::WritePrompt();
    return bOk;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  // MI print
  // "*stopped,reason=\"end-stepping-range\",frame={addr=\"0x%016" PRIx64
  // "\",func=\"%s\",args=[\"%s\"],file=\"%s\",fullname=\"%s\",line=\"%d\"},thread-id=\"%d\",stopped-threads=\"all\""

  // Function args
  CMICmnMIValueTuple miValueTuple;
  if (!rSessionInfo.MIResponseFormFrameInfo(
          thread, 0, CMICmnLLDBDebugSessionInfo::eFrameInfoFormat_AllArguments,
          miValueTuple))
    return MIstatus::failure;

  const CMICmnMIValueConst miValueConst("end-stepping-range");
  const CMICmnMIValueResult miValueResult("reason", miValueConst);
  CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult);
  const CMICmnMIValueResult miValueResult2("frame", miValueTuple);
  miOutOfBandRecord.Add(miValueResult2);

  // Add to MI thread-id=\"%d\",stopped-threads=\"all\"
  const CMIUtilString strThreadId(
      CMIUtilString::Format("%d", thread.GetIndexID()));
  const CMICmnMIValueConst miValueConst8(strThreadId);
  const CMICmnMIValueResult miValueResult8("thread-id", miValueConst8);
  miOutOfBandRecord.Add(miValueResult8);

  const CMICmnMIValueConst miValueConst9("all");
  const CMICmnMIValueResult miValueResult9("stopped-threads", miValueConst9);
  miOutOfBandRecord.Add(miValueResult9);
  bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
  bOk = bOk && CMICmnStreamStdout::WritePrompt();

  return bOk;
}

//++
// Details: Asynchronous function update selected thread.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::UpdateSelectedThread() {
  lldb::SBProcess process = CMICmnLLDBDebugSessionInfo::Instance()
                                .GetDebugger()
                                .GetSelectedTarget()
                                .GetProcess();
  if (!process.IsValid())
    return MIstatus::success;

  lldb::SBThread currentThread = process.GetSelectedThread();
  lldb::SBThread thread;
  const lldb::StopReason eCurrentThreadStoppedReason =
      currentThread.GetStopReason();
  if (!currentThread.IsValid() ||
      (eCurrentThreadStoppedReason == lldb::eStopReasonInvalid) ||
      (eCurrentThreadStoppedReason == lldb::eStopReasonNone)) {
    // Prefer a thread that has just completed its plan over another thread as
    // current thread
    lldb::SBThread planThread;
    lldb::SBThread otherThread;
    const size_t nThread = process.GetNumThreads();
    for (MIuint i = 0; i < nThread; i++) {
      //  GetThreadAtIndex() uses a base 0 index
      //  GetThreadByIndexID() uses a base 1 index
      thread = process.GetThreadAtIndex(i);
      const lldb::StopReason eThreadStopReason = thread.GetStopReason();
      switch (eThreadStopReason) {
      case lldb::eStopReasonTrace:
      case lldb::eStopReasonBreakpoint:
      case lldb::eStopReasonWatchpoint:
      case lldb::eStopReasonSignal:
      case lldb::eStopReasonException:
        if (!otherThread.IsValid())
          otherThread = thread;
        break;
      case lldb::eStopReasonPlanComplete:
        if (!planThread.IsValid())
          planThread = thread;
        break;
      case lldb::eStopReasonInvalid:
      case lldb::eStopReasonNone:
      default:
        break;
      }
    }
    if (planThread.IsValid())
      process.SetSelectedThread(planThread);
    else if (otherThread.IsValid())
      process.SetSelectedThread(otherThread);
    else {
      if (currentThread.IsValid())
        thread = currentThread;
      else
        thread = process.GetThreadAtIndex(0);

      if (thread.IsValid())
        process.SetSelectedThread(thread);
    }
  } // if( !currentThread.IsValid() || (eCurrentThreadStoppedReason ==
    // lldb::eStopReasonInvalid) || (eCurrentThreadStoppedReason ==
  // lldb::eStopReasonNone) )

  return MIstatus::success;
}

//++
// Details: Print to stdout "*running,thread-id=\"all\"", "(gdb)".
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStateRunning() {
  CMICmnMIValueConst miValueConst("all");
  CMICmnMIValueResult miValueResult("thread-id", miValueConst);
  CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_Running, miValueResult);
  bool bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
  bOk = bOk && CMICmnStreamStdout::WritePrompt();

  return bOk;
}

//++
// Details: Print to stdout "=thread-exited,id=\"%ld\",group-id=\"i1\"",
//                          "=thread-group-exited,id=\"i1\",exit-code=\"0\""),
//                          "*stopped,reason=\"exited-normally\"",
//                          "(gdb)"
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::HandleProcessEventStateExited() {
  const CMIUtilString strId(CMIUtilString::Format("%ld", 1));
  CMICmnMIValueConst miValueConst(strId);
  CMICmnMIValueResult miValueResult("id", miValueConst);
  CMICmnMIOutOfBandRecord miOutOfBandRecord(
      CMICmnMIOutOfBandRecord::eOutOfBand_ThreadExited, miValueResult);
  CMICmnMIValueConst miValueConst2("i1");
  CMICmnMIValueResult miValueResult2("group-id", miValueConst2);
  miOutOfBandRecord.Add(miValueResult2);
  bool bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
  if (bOk) {
    CMICmnMIValueConst miValueConst3("i1");
    CMICmnMIValueResult miValueResult3("id", miValueConst3);
    CMICmnMIOutOfBandRecord miOutOfBandRecord2(
        CMICmnMIOutOfBandRecord::eOutOfBand_ThreadGroupExited, miValueResult3);
    CMICmnMIValueConst miValueConst2("0");
    CMICmnMIValueResult miValueResult2("exit-code", miValueConst2);
    miOutOfBandRecord2.Add(miValueResult2);
    bOk = bOk && MiOutOfBandRecordToStdout(miOutOfBandRecord2);
  }
  if (bOk) {
    CMICmnMIValueConst miValueConst4("exited-normally");
    CMICmnMIValueResult miValueResult4("reason", miValueConst4);
    CMICmnMIOutOfBandRecord miOutOfBandRecord3(
        CMICmnMIOutOfBandRecord::eOutOfBand_Stopped, miValueResult4);
    bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord3);
  }
  bOk = bOk && CMICmnStreamStdout::WritePrompt();

  return bOk;
}

//++
// Details: Drain all stdout so we don't see any output come after we print our
// prompts.
//          The process has stuff waiting for stdout; get it and write it out to
//          the
//          appropriate place.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::GetProcessStdout() {
  CMIUtilString text;
  std::unique_ptr<char[]> apStdoutBuffer(new char[1024]);
  lldb::SBProcess process = CMICmnLLDBDebugSessionInfo::Instance()
                                .GetDebugger()
                                .GetSelectedTarget()
                                .GetProcess();
  while (1) {
    const size_t nBytes = process.GetSTDOUT(apStdoutBuffer.get(), 1024);
    text.append(apStdoutBuffer.get(), nBytes);

    while (1) {
      const size_t nNewLine = text.find('\n');
      if (nNewLine == std::string::npos)
        break;

      const CMIUtilString line(text.substr(0, nNewLine + 1));
      text.erase(0, nNewLine + 1);
      const bool bEscapeQuotes(true);
      CMICmnMIValueConst miValueConst(line.Escape(bEscapeQuotes));
      CMICmnMIOutOfBandRecord miOutOfBandRecord(
          CMICmnMIOutOfBandRecord::eOutOfBand_TargetStreamOutput, miValueConst);
      const bool bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
      if (!bOk)
        return MIstatus::failure;
    }

    if (nBytes == 0) {
      if (!text.empty()) {
        const bool bEscapeQuotes(true);
        CMICmnMIValueConst miValueConst(text.Escape(bEscapeQuotes));
        CMICmnMIOutOfBandRecord miOutOfBandRecord(
            CMICmnMIOutOfBandRecord::eOutOfBand_TargetStreamOutput,
            miValueConst);
        return MiOutOfBandRecordToStdout(miOutOfBandRecord);
      }
      break;
    }
  }

  return MIstatus::success;
}

//++
// Details: Drain all stderr so we don't see any output come after we print our
// prompts.
//          The process has stuff waiting for stderr; get it and write it out to
//          the
//          appropriate place.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::GetProcessStderr() {
  CMIUtilString text;
  std::unique_ptr<char[]> apStderrBuffer(new char[1024]);
  lldb::SBProcess process = CMICmnLLDBDebugSessionInfo::Instance()
                                .GetDebugger()
                                .GetSelectedTarget()
                                .GetProcess();
  while (1) {
    const size_t nBytes = process.GetSTDERR(apStderrBuffer.get(), 1024);
    text.append(apStderrBuffer.get(), nBytes);

    while (1) {
      const size_t nNewLine = text.find('\n');
      if (nNewLine == std::string::npos)
        break;

      const CMIUtilString line(text.substr(0, nNewLine + 1));
      const bool bEscapeQuotes(true);
      CMICmnMIValueConst miValueConst(line.Escape(bEscapeQuotes));
      CMICmnMIOutOfBandRecord miOutOfBandRecord(
          CMICmnMIOutOfBandRecord::eOutOfBand_TargetStreamOutput, miValueConst);
      const bool bOk = MiOutOfBandRecordToStdout(miOutOfBandRecord);
      if (!bOk)
        return MIstatus::failure;
    }

    if (nBytes == 0) {
      if (!text.empty()) {
        const bool bEscapeQuotes(true);
        CMICmnMIValueConst miValueConst(text.Escape(bEscapeQuotes));
        CMICmnMIOutOfBandRecord miOutOfBandRecord(
            CMICmnMIOutOfBandRecord::eOutOfBand_TargetStreamOutput,
            miValueConst);
        return MiOutOfBandRecordToStdout(miOutOfBandRecord);
      }
      break;
    }
  }

  return MIstatus::success;
}

//++
// Details: Asynchronous event function check for state changes.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::ChkForStateChanges() {
  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());
  lldb::SBProcess sbProcess = rSessionInfo.GetProcess();
  if (!sbProcess.IsValid())
    return MIstatus::success;

  // Check for created threads
  const MIuint nThread = sbProcess.GetNumThreads();
  for (MIuint i = 0; i < nThread; i++) {
    //  GetThreadAtIndex() uses a base 0 index
    //  GetThreadByIndexID() uses a base 1 index
    lldb::SBThread thread = sbProcess.GetThreadAtIndex(i);
    if (!thread.IsValid())
      continue;

    const MIuint threadIndexID = thread.GetIndexID();
    const bool bFound =
        std::find(rSessionInfo.m_vecActiveThreadId.cbegin(),
                  rSessionInfo.m_vecActiveThreadId.cend(),
                  threadIndexID) != rSessionInfo.m_vecActiveThreadId.end();
    if (!bFound) {
      rSessionInfo.m_vecActiveThreadId.push_back(threadIndexID);

      // Form MI "=thread-created,id=\"%d\",group-id=\"i1\""
      const CMIUtilString strValue(CMIUtilString::Format("%d", threadIndexID));
      const CMICmnMIValueConst miValueConst(strValue);
      const CMICmnMIValueResult miValueResult("id", miValueConst);
      CMICmnMIOutOfBandRecord miOutOfBand(
          CMICmnMIOutOfBandRecord::eOutOfBand_ThreadCreated, miValueResult);
      const CMICmnMIValueConst miValueConst2("i1");
      const CMICmnMIValueResult miValueResult2("group-id", miValueConst2);
      miOutOfBand.Add(miValueResult2);
      bool bOk = MiOutOfBandRecordToStdout(miOutOfBand);
      if (!bOk)
        return MIstatus::failure;
    }
  }

  lldb::SBThread currentThread = sbProcess.GetSelectedThread();
  if (currentThread.IsValid()) {
    const MIuint currentThreadIndexID = currentThread.GetIndexID();
    if (rSessionInfo.m_currentSelectedThread != currentThreadIndexID) {
      rSessionInfo.m_currentSelectedThread = currentThreadIndexID;

      // Form MI "=thread-selected,id=\"%d\""
      const CMIUtilString strValue(
          CMIUtilString::Format("%d", currentThreadIndexID));
      const CMICmnMIValueConst miValueConst(strValue);
      const CMICmnMIValueResult miValueResult("id", miValueConst);
      CMICmnMIOutOfBandRecord miOutOfBand(
          CMICmnMIOutOfBandRecord::eOutOfBand_ThreadSelected, miValueResult);
      if (!MiOutOfBandRecordToStdout(miOutOfBand))
        return MIstatus::failure;
    }
  }

  // Check for invalid (removed) threads
  CMICmnLLDBDebugSessionInfo::VecActiveThreadId_t::iterator it =
      rSessionInfo.m_vecActiveThreadId.begin();
  while (it != rSessionInfo.m_vecActiveThreadId.end()) {
    const MIuint threadIndexID = *it;
    lldb::SBThread thread = sbProcess.GetThreadByIndexID(threadIndexID);
    if (!thread.IsValid()) {
      // Form MI "=thread-exited,id=\"%ld\",group-id=\"i1\""
      const CMIUtilString strValue(CMIUtilString::Format("%ld", threadIndexID));
      const CMICmnMIValueConst miValueConst(strValue);
      const CMICmnMIValueResult miValueResult("id", miValueConst);
      CMICmnMIOutOfBandRecord miOutOfBand(
          CMICmnMIOutOfBandRecord::eOutOfBand_ThreadExited, miValueResult);
      const CMICmnMIValueConst miValueConst2("i1");
      const CMICmnMIValueResult miValueResult2("group-id", miValueConst2);
      miOutOfBand.Add(miValueResult2);
      bool bOk = MiOutOfBandRecordToStdout(miOutOfBand);
      if (!bOk)
        return MIstatus::failure;

      // Remove current thread from cache and get next
      it = rSessionInfo.m_vecActiveThreadId.erase(it);
    } else
      // Next
      ++it;
  }

  return CMICmnStreamStdout::WritePrompt();
}

//++
// Details: Take a fully formed MI result record and send to the stdout stream.
//          Also output to the MI Log file.
// Type:    Method.
// Args:    vrMiResultRecord  - (R) MI result record object.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::MiResultRecordToStdout(
    const CMICmnMIResultRecord &vrMiResultRecord) {
  return TextToStdout(vrMiResultRecord.GetString());
}

//++
// Details: Take a fully formed MI Out-of-band record and send to the stdout
// stream.
//          Also output to the MI Log file.
// Type:    Method.
// Args:    vrMiOutOfBandRecord - (R) MI Out-of-band record object.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::MiOutOfBandRecordToStdout(
    const CMICmnMIOutOfBandRecord &vrMiOutOfBandRecord) {
  return TextToStdout(vrMiOutOfBandRecord.GetString());
}

//++
// Details: Take a text data and send to the stdout stream. Also output to the
// MI Log
//          file.
// Type:    Method.
// Args:    vrTxt   - (R) Text.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::TextToStdout(const CMIUtilString &vrTxt) {
  return CMICmnStreamStdout::TextToStdout(vrTxt);
}

//++
// Details: Take a text data and send to the stderr stream. Also output to the
// MI Log
//          file.
// Type:    Method.
// Args:    vrTxt   - (R) Text.
// Return:  MIstatus::success - Functionality succeeded.
//          MIstatus::failure - Functionality failed.
// Throws:  None.
//--
bool CMICmnLLDBDebuggerHandleEvents::TextToStderr(const CMIUtilString &vrTxt) {
  return CMICmnStreamStderr::TextToStderr(vrTxt);
}

//++
// Details: Initialize the member variables with the signal values in this
// process
//          file.
// Type:    Method.
// Args:    None
// Return:  Noen
// Throws:  None.
//--
void CMICmnLLDBDebuggerHandleEvents::InitializeSignals() {
  if (!m_bSignalsInitialized) {
    lldb::SBProcess sbProcess =
        CMICmnLLDBDebugSessionInfo::Instance().GetProcess();
    if (sbProcess.IsValid()) {
      lldb::SBUnixSignals unix_signals = sbProcess.GetUnixSignals();
      m_SIGINT = unix_signals.GetSignalNumberFromName("SIGINT");
      m_SIGSTOP = unix_signals.GetSignalNumberFromName("SIGSTOP");
      m_SIGSEGV = unix_signals.GetSignalNumberFromName("SIGSEGV");
      m_SIGTRAP = unix_signals.GetSignalNumberFromName("SIGTRAP");
      m_bSignalsInitialized = true;
    }
  }
}
