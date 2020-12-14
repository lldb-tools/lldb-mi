//===-- MICmdCmdBreak.cpp ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Overview:    CMICmdCmdBreakInsert            implementation.
//              CMICmdCmdBreakDelete            implementation.
//              CMICmdCmdBreakDisable           implementation.
//              CMICmdCmdBreakEnable            implementation.
//              CMICmdCmdBreakAfter             implementation.
//              CMICmdCmdBreakCondition         implementation.

// Third Party Headers:
#include "cassert"
#include "lldb/API/SBBreakpointLocation.h"
#include "lldb/API/SBThread.h"

// In-house headers:
#include "MICmdArgValFile.h"
#include "MICmdArgValListOfN.h"
#include "MICmdArgValNumber.h"
#include "MICmdArgValOptionLong.h"
#include "MICmdArgValOptionShort.h"
#include "MICmdArgValString.h"
#include "MICmdArgValThreadGrp.h"
#include "MICmdCmdBreak.h"
#include "MICmnLLDBDebugger.h"
#include "MICmnMIOutOfBandRecord.h"
#include "MICmnMIResultRecord.h"
#include "MICmnMIValueConst.h"
#include "MICmnStreamStdout.h"

//++
// Details: CMICmdCmdBreakInsert constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakInsert::CMICmdCmdBreakInsert()
    : m_bBreakpointIsTemp(false), m_bBreakpointIsPending(false),
      m_nBreakpointIgnoreCount(0), m_bBreakpointEnabled(false),
      m_bBreakpointCondition(false), m_bBreakpointThreadId(false),
      m_nBreakpointThreadId(0), m_constStrArgNamedTempBreakpoint("t"),
      m_constStrArgNamedHWBreakpoint("h"),
      m_constStrArgNamedPendinfBreakpoint("f"),
      m_constStrArgNamedDisableBreakpoint("d"), m_constStrArgNamedTracePt("a"),
      m_constStrArgNamedConditionalBreakpoint("c"),
      m_constStrArgNamedInoreCnt("i"),
      m_constStrArgNamedRestrictBreakpointToThreadId("p"),
      m_constStrArgNamedLocation("location") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "break-insert";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdBreakInsert::CreateSelf;
}

//++
// Details: CMICmdCmdBreakInsert destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakInsert::~CMICmdCmdBreakInsert() {}

//++
// Details: The invoker requires this function. The parses the command line
// options
//          arguments to extract values for each of those arguments.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakInsert::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(m_constStrArgNamedTempBreakpoint,
                                               false, true));
  // Not implemented m_setCmdArgs.Add(new CMICmdArgValOptionShort(
  // m_constStrArgNamedHWBreakpoint, false, false));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(
      m_constStrArgNamedPendinfBreakpoint, false, true));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(
      m_constStrArgNamedDisableBreakpoint, false, false));
  // Not implemented m_setCmdArgs.Add(new CMICmdArgValOptionShort(
  // m_constStrArgNamedTracePt, false, false));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(
      m_constStrArgNamedConditionalBreakpoint, false, true,
      CMICmdArgValListBase::eArgValType_StringQuoted, 1));
  m_setCmdArgs.Add(
      new CMICmdArgValOptionShort(m_constStrArgNamedInoreCnt, false, true,
                                  CMICmdArgValListBase::eArgValType_Number, 1));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(
      m_constStrArgNamedRestrictBreakpointToThreadId, false, true,
      CMICmdArgValListBase::eArgValType_Number, 1));
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgNamedLocation, false,
                                          true, false, false, true));
  return ParseValidateCmdOptions();
}

//++
// Helper function for CMICmdCmdBreakInsert::Execute().
//
// Given a string, return the position of the ':' separator in 'file:func'
// or 'file:line', if any.  If not found, return npos.  For example, return
// 5 for 'foo.c:std::string'.
//--
static size_t findFileSeparatorPos(const std::string &x) {
  // Full paths in windows can have ':' after a drive letter, so we
  // search backwards, taking care to skip C++ namespace tokens '::'.
  size_t n = x.rfind(':');
  while (n != std::string::npos && n > 1 && x[n - 1] == ':') {
    n = x.rfind(':', n - 2);
  }
  return n;
}

//++
// Details: The invoker requires this function. The command does work in this
// function.
//          The command is likely to communicate with the LLDB SBDebugger in
//          here.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakInsert::Execute() {
  CMICMDBASE_GETOPTION(pArgTempBreakpoint, OptionShort,
                       m_constStrArgNamedTempBreakpoint);
  CMICMDBASE_GETOPTION(pArgThreadGroup, OptionLong, m_constStrArgThreadGroup);
  CMICMDBASE_GETOPTION(pArgLocation, String, m_constStrArgNamedLocation);
  CMICMDBASE_GETOPTION(pArgIgnoreCnt, OptionShort, m_constStrArgNamedInoreCnt);
  CMICMDBASE_GETOPTION(pArgPendingBreakpoint, OptionShort,
                       m_constStrArgNamedPendinfBreakpoint);
  CMICMDBASE_GETOPTION(pArgDisableBreakpoint, OptionShort,
                       m_constStrArgNamedDisableBreakpoint);
  CMICMDBASE_GETOPTION(pArgConditionalBreakpoint, OptionShort,
                       m_constStrArgNamedConditionalBreakpoint);
  CMICMDBASE_GETOPTION(pArgRestrictBreakpointToThreadId, OptionShort,
                       m_constStrArgNamedRestrictBreakpointToThreadId);

  // Ask LLDB for the target to check if we have valid or dummy one.
  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());
  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();

  m_bBreakpointEnabled = !pArgDisableBreakpoint->GetFound();
  m_bBreakpointIsTemp = pArgTempBreakpoint->GetFound();
  m_bHaveArgOptionThreadGrp = pArgThreadGroup->GetFound();
  if (m_bHaveArgOptionThreadGrp) {
    MIuint nThreadGrp = 0;
    pArgThreadGroup->GetExpectedOption<CMICmdArgValThreadGrp, MIuint>(
        nThreadGrp);
    m_strArgOptionThreadGrp = CMIUtilString::Format("i%d", nThreadGrp);
  }

  if (sbTarget == rSessionInfo.GetDebugger().GetDummyTarget())
    m_bBreakpointIsPending = true;
  else {
    m_bBreakpointIsPending = pArgPendingBreakpoint->GetFound();
    if (!m_bBreakpointIsPending) {
      CMIUtilString pending;
      if (m_rLLDBDebugSessionInfo.SharedDataRetrieve("breakpoint.pending",
                                                     pending)) {
        m_bBreakpointIsPending = pending == "on";
      }
    }
  }

  if (pArgLocation->GetFound())
    m_brkName = pArgLocation->GetValue();
  else if (m_bBreakpointIsPending) {
    pArgPendingBreakpoint->GetExpectedOption<CMICmdArgValString, CMIUtilString>(
        m_brkName);
  }
  if (pArgIgnoreCnt->GetFound()) {
    pArgIgnoreCnt->GetExpectedOption<CMICmdArgValNumber, MIuint>(
        m_nBreakpointIgnoreCount);
  }
  m_bBreakpointCondition = pArgConditionalBreakpoint->GetFound();
  if (m_bBreakpointCondition) {
    pArgConditionalBreakpoint
        ->GetExpectedOption<CMICmdArgValString, CMIUtilString>(
            m_breakpointCondition);
  }
  m_bBreakpointThreadId = pArgRestrictBreakpointToThreadId->GetFound();
  if (m_bBreakpointCondition) {
    pArgRestrictBreakpointToThreadId
        ->GetExpectedOption<CMICmdArgValNumber, MIuint>(m_nBreakpointThreadId);
  }

  // Determine if break on a file line or at a function
  BreakPoint_e eBreakpointType = eBreakPoint_NotDefineYet;
  CMIUtilString fileName;
  MIuint nFileLine = 0;
  CMIUtilString strFileFn;
  CMIUtilString rStrLineOrFn;
  // Is the string in the form 'file:func' or 'file:line'?
  // If so, find the position of the ':' separator.
  const size_t nPosColon = findFileSeparatorPos(m_brkName);
  if (nPosColon != std::string::npos) {
    // Extract file name and line number from it
    fileName = m_brkName.substr(0, nPosColon);
    rStrLineOrFn =
        m_brkName.substr(nPosColon + 1, m_brkName.size() - nPosColon - 1);

    if (rStrLineOrFn.empty())
      eBreakpointType = eBreakPoint_ByName;
    else {
      MIint64 nValue = 0;
      if (rStrLineOrFn.ExtractNumber(nValue)) {
        nFileLine = static_cast<MIuint>(nValue);
        eBreakpointType = eBreakPoint_ByFileLine;
      } else {
        strFileFn = rStrLineOrFn;
        eBreakpointType = eBreakPoint_ByFileFn;
      }
    }
  }

  // Determine if break defined as an address
  lldb::addr_t nAddress = 0;
  if (eBreakpointType == eBreakPoint_NotDefineYet) {
    if (!m_brkName.empty() && m_brkName[0] == '*') {
      MIint64 nValue = 0;
      if (CMIUtilString(m_brkName.substr(1)).ExtractNumber(nValue)) {
        nAddress = static_cast<lldb::addr_t>(nValue);
        eBreakpointType = eBreakPoint_ByAddress;
      }
    }
  }

  // Break defined as an function
  if (eBreakpointType == eBreakPoint_NotDefineYet) {
    eBreakpointType = eBreakPoint_ByName;
  }

  // Ask LLDB to create a breakpoint
  bool bOk = MIstatus::success;
  switch (eBreakpointType) {
  case eBreakPoint_ByAddress:
    m_breakpoint = sbTarget.BreakpointCreateByAddress(nAddress);
    break;
  case eBreakPoint_ByFileFn: {
    lldb::SBFileSpecList module; // search in all modules
    lldb::SBFileSpecList compUnit;
    compUnit.Append(lldb::SBFileSpec(fileName.c_str()));
    m_breakpoint =
        sbTarget.BreakpointCreateByName(strFileFn.c_str(), module, compUnit);
    break;
  }
  case eBreakPoint_ByFileLine:
    m_breakpoint =
        sbTarget.BreakpointCreateByLocation(fileName.c_str(), nFileLine);
    break;
  case eBreakPoint_ByName:
    m_breakpoint = sbTarget.BreakpointCreateByName(m_brkName.c_str(), nullptr);
    break;
  case eBreakPoint_count:
  case eBreakPoint_NotDefineYet:
  case eBreakPoint_Invalid:
    bOk = MIstatus::failure;
    break;
  }

  if (bOk) {
    if (!m_bBreakpointIsPending && (m_breakpoint.GetNumLocations() == 0)) {
      sbTarget.BreakpointDelete(m_breakpoint.GetID());
      SetError(CMIUtilString::Format(
          MIRSRC(IDS_CMD_ERR_BREAKPOINT_LOCATION_NOT_FOUND),
          m_cmdData.strMiCmd.c_str(), m_brkName.c_str()));
      return MIstatus::failure;
    }

    m_breakpoint.SetEnabled(m_bBreakpointEnabled);
    m_breakpoint.SetIgnoreCount(m_nBreakpointIgnoreCount);
    m_breakpoint.SetOneShot(m_bBreakpointIsTemp);
    if (m_bBreakpointCondition)
      m_breakpoint.SetCondition(m_breakpointCondition.c_str());
    if (m_bBreakpointThreadId)
      m_breakpoint.SetThreadID(m_nBreakpointThreadId);
  }

  // CODETAG_LLDB_BREAKPOINT_CREATION
  // This is in the main thread
  // Record break point information to be by LLDB event handler function
  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.GetStoppointInfo(m_breakpoint, sStoppointInfo))
    return MIstatus::failure;

  sStoppointInfo.m_bDisp = m_bBreakpointIsTemp;
  sStoppointInfo.m_bEnabled = m_bBreakpointEnabled;
  sStoppointInfo.m_bHaveArgOptionThreadGrp = m_bHaveArgOptionThreadGrp;
  sStoppointInfo.m_strOptThrdGrp = m_strArgOptionThreadGrp;
  sStoppointInfo.m_nTimes = m_breakpoint.GetHitCount();
  sStoppointInfo.m_strOrigLoc = m_brkName;
  sStoppointInfo.m_nIgnore = m_nBreakpointIgnoreCount;
  sStoppointInfo.m_bPending = m_bBreakpointIsPending;
  sStoppointInfo.m_bCondition = m_bBreakpointCondition;
  sStoppointInfo.m_strCondition = m_breakpointCondition;
  sStoppointInfo.m_bBreakpointThreadId = m_bBreakpointThreadId;
  sStoppointInfo.m_nBreakpointThreadId = m_nBreakpointThreadId;

  bOk = bOk && rSessionInfo.RecordStoppointInfo(sStoppointInfo);
  if (!bOk) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_brkName.c_str()));
    return MIstatus::failure;
  }

  // CODETAG_LLDB_STOPPOINT_ID_MAX
  if (sStoppointInfo.m_nMiId > rSessionInfo.m_nBreakpointCntMax) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_CNT_EXCEEDED), m_cmdData.strMiCmd.c_str(),
        static_cast<uint64_t>(rSessionInfo.m_nBreakpointCntMax),
        static_cast<uint64_t>(sStoppointInfo.m_nMiId)));
    return MIstatus::failure;
  }

  return MIstatus::success;
}

//++
// Details: The invoker requires this function. The command prepares a MI Record
// Result
//          for the work carried out in the Execute().
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakInsert::Acknowledge() {
  // Get breakpoint information
  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  MIuint nMiStoppointId =
      CMICmnLLDBDebugSessionInfo::Instance().GetOrCreateMiStoppointId(
          m_breakpoint.GetID(),
          CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint);

  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(nMiStoppointId)));
    return MIstatus::failure;
  }

  // MI print
  // "^done,bkpt={number=\"%d\",type=\"breakpoint\",disp=\"%s\",enabled=\"%c\",addr=\"0x%016"
  // PRIx64
  // "\",func=\"%s\",file=\"%s\",fullname=\"%s/%s\",line=\"%d\",thread-groups=[\"%s\"],times=\"%d\",original-location=\"%s\"}"
  CMICmnMIValueTuple miValueTuple;
  if (!rSessionInfo.MIResponseFormBreakpointInfo(sStoppointInfo, miValueTuple))
    return MIstatus::failure;

  const CMICmnMIValueResult miValueResultD("bkpt", miValueTuple);
  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
      miValueResultD);
  m_miResultRecord = miRecordResult;

  return MIstatus::success;
}

//++
// Details: Required by the CMICmdFactory when registering *this command. The
// factory
//          calls this function to create an instance of *this command.
// Type:    Static method.
// Args:    None.
// Return:  CMICmdBase * - Pointer to a new command.
// Throws:  None.
//--
CMICmdBase *CMICmdCmdBreakInsert::CreateSelf() {
  return new CMICmdCmdBreakInsert();
}

//++
// Details: CMICmdCmdBreakDelete constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakDelete::CMICmdCmdBreakDelete()
    : m_constStrArgNamedBreakpoint("breakpoint") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "break-delete";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdBreakDelete::CreateSelf;
}

//++
// Details: CMICmdCmdBreakDelete destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakDelete::~CMICmdCmdBreakDelete() {}

//++
// Details: The invoker requires this function. The parses the command line
// options
//          arguments to extract values for each of those arguments.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakDelete::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValListOfN(m_constStrArgNamedBreakpoint, true, true,
                              CMICmdArgValListBase::eArgValType_Number));
  return ParseValidateCmdOptions();
}

//++
// Details: The invoker requires this function. The command does work in this
// function.
//          The command is likely to communicate with the LLDB SBDebugger in
//          here.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakDelete::Execute() {
  CMICMDBASE_GETOPTION(pArgBreakpoint, ListOfN, m_constStrArgNamedBreakpoint);

  // ATM we only handle one break point ID
  MIuint nMiStoppointId;
  if (!pArgBreakpoint->GetExpectedOption<CMICmdArgValNumber, MIuint>(
          nMiStoppointId)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgNamedBreakpoint.c_str()));
    return MIstatus::failure;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(nMiStoppointId)));
    return MIstatus::failure;
  }

  bool bSuccess = false;
  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStoppointInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint)
    bSuccess = sbTarget.BreakpointDelete(
        static_cast<lldb::break_id_t>(sStoppointInfo.m_nLldbId));
  else
    bSuccess = sbTarget.DeleteWatchpoint(
        static_cast<lldb::watch_id_t>(sStoppointInfo.m_nLldbId));

  if (!bSuccess) {
    const CMIUtilString strBrkNum(CMIUtilString::Format(
        "%" PRIu64, static_cast<uint64_t>(nMiStoppointId)));
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   strBrkNum.c_str()));
    return MIstatus::failure;
  }

  return MIstatus::success;
}

//++
// Details: The invoker requires this function. The command prepares a MI Record
// Result
//          for the work carried out in the Execute().
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakDelete::Acknowledge() {
  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done);
  m_miResultRecord = miRecordResult;

  return MIstatus::success;
}

//++
// Details: Required by the CMICmdFactory when registering *this command. The
// factory
//          calls this function to create an instance of *this command.
// Type:    Static method.
// Args:    None.
// Return:  CMICmdBase * - Pointer to a new command.
// Throws:  None.
//--
CMICmdBase *CMICmdCmdBreakDelete::CreateSelf() {
  return new CMICmdCmdBreakDelete();
}

//++
// Details: CMICmdCmdBreakDisable constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakDisable::CMICmdCmdBreakDisable()
    : m_constStrArgNamedBreakpoint("breakpoint"),
      m_bBreakpointDisabledOk(false), m_nMiStoppointId(0) {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "break-disable";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdBreakDisable::CreateSelf;
}

//++
// Details: CMICmdCmdBreakDisable destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakDisable::~CMICmdCmdBreakDisable() {}

//++
// Details: The invoker requires this function. The parses the command line
// options
//          arguments to extract values for each of those arguments.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakDisable::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValListOfN(m_constStrArgNamedBreakpoint, true, true,
                              CMICmdArgValListBase::eArgValType_Number));
  return ParseValidateCmdOptions();
}

//++
// Details: The invoker requires this function. The command does work in this
// function.
//          The command is likely to communicate with the LLDB SBDebugger in
//          here.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakDisable::Execute() {
  CMICMDBASE_GETOPTION(pArgBreakpoint, ListOfN, m_constStrArgNamedBreakpoint);

  // ATM we only handle one break point ID
  if (!pArgBreakpoint->GetExpectedOption<CMICmdArgValNumber, MIuint>(
          m_nMiStoppointId)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgNamedBreakpoint.c_str()));
    return MIstatus::failure;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(m_nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStoppointId)));
    return MIstatus::failure;
  }

  auto disable = [&m_bBreakpointDisabledOk =
                      m_bBreakpointDisabledOk](auto &vrPt) {
    if (vrPt.IsValid()) {
      m_bBreakpointDisabledOk = true;
      vrPt.SetEnabled(false);
    }
  };

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStoppointInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint breakpoint = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStoppointInfo.m_nLldbId));
    disable(breakpoint);
  } else {
    lldb::SBWatchpoint watchpoint = sbTarget.FindWatchpointByID(
        static_cast<lldb::watch_id_t>(sStoppointInfo.m_nLldbId));
    disable(watchpoint);
  }

  return MIstatus::success;
}

//++
// Details: The invoker requires this function. The command prepares a MI Record
// Result
//          for the work carried out in the Execute().
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakDisable::Acknowledge() {
  if (m_bBreakpointDisabledOk) {
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done);
    m_miResultRecord = miRecordResult;
    return MIstatus::success;
  }

  const CMIUtilString strBreakpointId(CMIUtilString::Format(
      "%" PRIu64, static_cast<uint64_t>(m_nMiStoppointId)));
  const CMICmnMIValueConst miValueConst(CMIUtilString::Format(
      MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID), strBreakpointId.c_str()));
  const CMICmnMIValueResult miValueResult("msg", miValueConst);
  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Error,
      miValueResult);
  m_miResultRecord = miRecordResult;

  return MIstatus::success;
}

//++
// Details: Required by the CMICmdFactory when registering *this command. The
// factory
//          calls this function to create an instance of *this command.
// Type:    Static method.
// Args:    None.
// Return:  CMICmdBase * - Pointer to a new command.
// Throws:  None.
//--
CMICmdBase *CMICmdCmdBreakDisable::CreateSelf() {
  return new CMICmdCmdBreakDisable();
}

//++
// Details: CMICmdCmdBreakEnable constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakEnable::CMICmdCmdBreakEnable()
    : m_constStrArgNamedBreakpoint("breakpoint"), m_bBreakpointEnabledOk(false),
      m_nMiStoppointId(0) {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "break-enable";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdBreakEnable::CreateSelf;
}

//++
// Details: CMICmdCmdBreakEnable destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakEnable::~CMICmdCmdBreakEnable() {}

//++
// Details: The invoker requires this function. The parses the command line
// options
//          arguments to extract values for each of those arguments.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakEnable::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValListOfN(m_constStrArgNamedBreakpoint, true, true,
                              CMICmdArgValListBase::eArgValType_Number));
  return ParseValidateCmdOptions();
}

//++
// Details: The invoker requires this function. The command does work in this
// function.
//          The command is likely to communicate with the LLDB SBDebugger in
//          here.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakEnable::Execute() {
  CMICMDBASE_GETOPTION(pArgBreakpoint, ListOfN, m_constStrArgNamedBreakpoint);

  // ATM we only handle one break point ID
  if (!pArgBreakpoint->GetExpectedOption<CMICmdArgValNumber, MIuint>(
          m_nMiStoppointId)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgNamedBreakpoint.c_str()));
    return MIstatus::failure;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(m_nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStoppointId)));
    return MIstatus::failure;
  }

  auto enable = [&m_bBreakpointEnabledOk = m_bBreakpointEnabledOk](auto &vrPt) {
    if (vrPt.IsValid()) {
      m_bBreakpointEnabledOk = true;
      vrPt.SetEnabled(true);
    }
  };

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStoppointInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint breakpoint = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStoppointInfo.m_nLldbId));
    enable(breakpoint);
  } else {
    lldb::SBWatchpoint watchpoint = sbTarget.FindWatchpointByID(
        static_cast<lldb::watch_id_t>(sStoppointInfo.m_nLldbId));
    enable(watchpoint);
  }

  return MIstatus::success;
}

//++
// Details: The invoker requires this function. The command prepares a MI Record
// Result
//          for the work carried out in the Execute().
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakEnable::Acknowledge() {
  if (m_bBreakpointEnabledOk) {
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done);
    m_miResultRecord = miRecordResult;
    return MIstatus::success;
  }

  const CMIUtilString strBreakpointId(CMIUtilString::Format(
      "%" PRIu64, static_cast<uint64_t>(m_nMiStoppointId)));
  const CMICmnMIValueConst miValueConst(CMIUtilString::Format(
      MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID), strBreakpointId.c_str()));
  const CMICmnMIValueResult miValueResult("msg", miValueConst);
  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Error,
      miValueResult);
  m_miResultRecord = miRecordResult;

  return MIstatus::success;
}

//++
// Details: Required by the CMICmdFactory when registering *this command. The
// factory
//          calls this function to create an instance of *this command.
// Type:    Static method.
// Args:    None.
// Return:  CMICmdBase * - Pointer to a new command.
// Throws:  None.
//--
CMICmdBase *CMICmdCmdBreakEnable::CreateSelf() {
  return new CMICmdCmdBreakEnable();
}

//++
// Details: CMICmdCmdBreakAfter constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakAfter::CMICmdCmdBreakAfter()
    : m_constStrArgNamedNumber("number"), m_constStrArgNamedCount("count"),
      m_nMiStoppointId(0), m_nBreakpointCount(0) {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "break-after";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdBreakAfter::CreateSelf;
}

//++
// Details: CMICmdCmdBreakAfter destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakAfter::~CMICmdCmdBreakAfter() {}

//++
// Details: The invoker requires this function. The parses the command line
// options
//          arguments to extract values for each of those arguments.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakAfter::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValNumber(m_constStrArgNamedNumber, true, true));
  m_setCmdArgs.Add(new CMICmdArgValNumber(m_constStrArgNamedCount, true, true));
  return ParseValidateCmdOptions();
}

//++
// Details: The invoker requires this function. The command does work in this
// function.
//          The command is likely to communicate with the LLDB SBDebugger in
//          here.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakAfter::Execute() {
  CMICMDBASE_GETOPTION(pArgNumber, Number, m_constStrArgNamedNumber);
  CMICMDBASE_GETOPTION(pArgCount, Number, m_constStrArgNamedCount);

  m_nMiStoppointId = static_cast<MIuint>(pArgNumber->GetValue());
  m_nBreakpointCount = pArgCount->GetValue();

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(m_nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStoppointId)));
    return MIstatus::failure;
  }

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStoppointInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint breakpoint = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStoppointInfo.m_nLldbId));
    return SetIgnoreCount(rSessionInfo, breakpoint);
  }

  lldb::SBWatchpoint watchpoint = sbTarget.FindWatchpointByID(
      static_cast<lldb::watch_id_t>(sStoppointInfo.m_nLldbId));
  return SetIgnoreCount(rSessionInfo, watchpoint);
}

//++
// Details: The invoker requires this function. The command prepares a MI Record
// Result
//          for the work carried out in the Execute().
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakAfter::Acknowledge() {
  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done);
  m_miResultRecord = miRecordResult;

  return MIstatus::success;
}

//++
// Details: Required by the CMICmdFactory when registering *this command. The
// factory
//          calls this function to create an instance of *this command.
// Type:    Static method.
// Args:    None.
// Return:  CMICmdBase * - Pointer to a new command.
// Throws:  None.
//--
CMICmdBase *CMICmdCmdBreakAfter::CreateSelf() {
  return new CMICmdCmdBreakAfter();
}

//++
// Details: Find a stop info corresponding to the specified breakpoint and
//          record the new ignore count.
// Type:    Method.
// Args:    rSessionInfo - (R)  The current session info.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakAfter::UpdateStoppointInfo(
    CMICmnLLDBDebugSessionInfo &rSessionInfo) {
  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(m_nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStoppointId)));
    return MIstatus::failure;
  }
  sStoppointInfo.m_nIgnore = m_nBreakpointCount;
  return rSessionInfo.RecordStoppointInfo(sStoppointInfo);
}

//++
// Details: CMICmdCmdBreakCondition constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakCondition::CMICmdCmdBreakCondition()
    : m_constStrArgNamedNumber("number"), m_constStrArgNamedExpr("expr"),
      m_constStrArgNamedExprNoQuotes(
          "expression not surround by quotes") // Not specified in MI spec, we
                                               // need to handle expressions not
                                               // surrounded by quotes
      ,
      m_nMiStoppointId(0) {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "break-condition";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdBreakCondition::CreateSelf;
}

//++
// Details: CMICmdCmdBreakCondition destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakCondition::~CMICmdCmdBreakCondition() {}

//++
// Details: The invoker requires this function. The parses the command line
// options
//          arguments to extract values for each of those arguments.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakCondition::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValNumber(m_constStrArgNamedNumber, true, true));
  m_setCmdArgs.Add(
      new CMICmdArgValString(m_constStrArgNamedExpr, true, true, true, true));
  m_setCmdArgs.Add(new CMICmdArgValListOfN(
      m_constStrArgNamedExprNoQuotes, false, false,
      CMICmdArgValListBase::eArgValType_StringQuotedNumber));
  return ParseValidateCmdOptions();
}

//++
// Details: The invoker requires this function. The command does work in this
// function.
//          The command is likely to communicate with the LLDB SBDebugger in
//          here.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakCondition::Execute() {
  CMICMDBASE_GETOPTION(pArgNumber, Number, m_constStrArgNamedNumber);
  CMICMDBASE_GETOPTION(pArgExpr, String, m_constStrArgNamedExpr);

  m_nMiStoppointId = static_cast<MIuint>(pArgNumber->GetValue());
  m_strBreakpointExpr = pArgExpr->GetValue();
  m_strBreakpointExpr += GetRestOfExpressionNotSurroundedInQuotes();

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(m_nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStoppointId)));
    return MIstatus::failure;
  }

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStoppointInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint breakpoint = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStoppointInfo.m_nLldbId));
    return SetCondition(rSessionInfo, breakpoint);
  }

  lldb::SBWatchpoint watchpoint = sbTarget.FindWatchpointByID(
      static_cast<lldb::watch_id_t>(sStoppointInfo.m_nLldbId));
  return SetCondition(rSessionInfo, watchpoint);
}

//++
// Details: The invoker requires this function. The command prepares a MI Record
// Result
//          for the work carried out in the Execute().
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakCondition::Acknowledge() {
  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done);
  m_miResultRecord = miRecordResult;

  return MIstatus::success;
}

//++
// Details: Required by the CMICmdFactory when registering *this command. The
// factory
//          calls this function to create an instance of *this command.
// Type:    Static method.
// Args:    None.
// Return:  CMICmdBase * - Pointer to a new command.
// Throws:  None.
//--
CMICmdBase *CMICmdCmdBreakCondition::CreateSelf() {
  return new CMICmdCmdBreakCondition();
}

//++
// Details: A breakpoint expression can be passed to *this command as:
//              a single string i.e. '2' -> ok.
//              a quoted string i.e. "a > 100" -> ok
//              a non quoted string i.e. 'a > 100' -> not ok
//          CMICmdArgValString only extracts the first space separated string,
//          the "a".
//          This function using the optional argument type CMICmdArgValListOfN
//          collects
//          the rest of the expression so that is may be added to the 'a' part
//          to form a
//          complete expression string i.e. "a > 100".
//          If the expression value was guaranteed to be surrounded by quotes
//          them this
//          function would not be necessary.
// Type:    Method.
// Args:    None.
// Return:  CMIUtilString - Rest of the breakpoint expression.
// Throws:  None.
//--
CMIUtilString
CMICmdCmdBreakCondition::GetRestOfExpressionNotSurroundedInQuotes() {
  CMIUtilString strExpression;

  CMICmdArgValListOfN *pArgExprNoQuotes =
      CMICmdBase::GetOption<CMICmdArgValListOfN>(
          m_constStrArgNamedExprNoQuotes);
  if (pArgExprNoQuotes != nullptr) {
    const CMICmdArgValListBase::VecArgObjPtr_t &rVecExprParts(
        pArgExprNoQuotes->GetExpectedOptions());
    if (!rVecExprParts.empty()) {
      CMICmdArgValListBase::VecArgObjPtr_t::const_iterator it =
          rVecExprParts.begin();
      while (it != rVecExprParts.end()) {
        const CMICmdArgValString *pPartExpr =
            static_cast<CMICmdArgValString *>(*it);
        const CMIUtilString &rPartExpr = pPartExpr->GetValue();
        strExpression += " ";
        strExpression += rPartExpr;

        // Next
        ++it;
      }
      strExpression = strExpression.Trim();
    }
  }

  return strExpression;
}

//++
// Details: Find a stoppoint info corresponding to the specified stoppoint and
//          record the new condition.
// Type:    Method.
// Args:    rSessionInfo - (R)  The current session info.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakCondition::UpdateStoppointInfo(
    CMICmnLLDBDebugSessionInfo &rSessionInfo) {
  CMICmnLLDBDebugSessionInfo::SStoppointInfo sStoppointInfo;
  if (!rSessionInfo.RecordStoppointInfoGet(m_nMiStoppointId, sStoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStoppointId)));
    return MIstatus::failure;
  }
  sStoppointInfo.m_strCondition = m_strBreakpointExpr;
  return rSessionInfo.RecordStoppointInfo(sStoppointInfo);
}

// Details: CMICmdCmdBreakWatch constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdBreakWatch::CMICmdCmdBreakWatch()
    : m_constStrArgNamedAccessWatchpoint("a"),
      m_constStrArgNamedReadWatchpoint("r"), m_constStrArgNamedExpr("expr"),
      m_stoppointInfo(), m_watchpoint() {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "break-watch";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdBreakWatch::CreateSelf;
}

//++
// Details: The invoker requires this function. The parses the command line
//          options arguments to extract values for each of those arguments.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakWatch::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(
      m_constStrArgNamedAccessWatchpoint, false, true));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(m_constStrArgNamedReadWatchpoint,
                                               false, true));
  m_setCmdArgs.Add(new CMICmdArgValText(m_constStrArgNamedExpr, true, true));
  return ParseValidateCmdOptions();
}

static bool FindLocalVariableAddress(lldb::SBTarget &rSbTarget,
                                     lldb::SBFrame &rSbFrame,
                                     const CMIUtilString &rExpression,
                                     lldb::addr_t &rAddress, size_t &rSize) {
  auto sbVariableValue = rSbFrame.GetValueForVariablePath(
      rExpression.c_str(), lldb::eNoDynamicValues);
  auto sbAddress = sbVariableValue.GetAddress();

  bool isValid = sbVariableValue && sbAddress;
  if (isValid) {
    rAddress = sbAddress.GetLoadAddress(rSbTarget);
    rSize = sbVariableValue.GetByteSize();
  }

  return isValid;
}

static bool FindGlobalVariableAddress(lldb::SBTarget &rSbTarget,
                                      lldb::SBFrame &rSbFrame,
                                      const CMIUtilString &rExpression,
                                      lldb::addr_t &rAddress, size_t &rSize) {
  auto sbGlobalVariableValue =
      rSbTarget.FindFirstGlobalVariable(rExpression.c_str());
  if (sbGlobalVariableValue.IsValid()) {
    auto sbAddress = sbGlobalVariableValue.GetAddress();
    if (sbAddress.IsValid()) {
      rAddress = sbAddress.GetLoadAddress(rSbTarget);
      rSize = sbGlobalVariableValue.GetByteSize();
      return MIstatus::success;
    }
  }

  // In case the previous part didn't succeed, the expression must be something
  // like "a.b". For locally-visible variables, there is
  // SBFrame::GetValueForVariablePath that can handle this kind of expressions
  // but there is no any analogue of this function for global variables. So, we
  // have to try an address expression at least.
  auto addressExpression = "&(" + rExpression + ")";
  auto sbExpressionValue =
      rSbFrame.EvaluateExpression(addressExpression.c_str());

  lldb::SBError sbError;
  rAddress =
      static_cast<lldb::addr_t>(sbExpressionValue.GetValueAsUnsigned(sbError));
  if (sbError.Fail())
    return false;

  assert(sbExpressionValue.TypeIsPointerType());
  rSize = sbExpressionValue.GetType().GetPointeeType().GetByteSize();

  return true;
}

static bool FindAddressByExpressionEvaluation(lldb::SBTarget &rSbTarget,
                                              lldb::SBFrame &rSbFrame,
                                              const CMIUtilString &rExpression,
                                              lldb::addr_t &rAddress,
                                              size_t &rSize) {
  auto sbExpressionValue = rSbFrame.EvaluateExpression(rExpression.c_str());

  lldb::SBError sbError;
  rAddress =
      static_cast<lldb::addr_t>(sbExpressionValue.GetValueAsUnsigned(sbError));
  if (sbError.Fail())
    return false;

  if (sbExpressionValue.TypeIsPointerType())
    rSize = sbExpressionValue.GetType().GetPointeeType().GetByteSize();
  else
    rSize = rSbTarget.GetDataByteSize();

  return true;
}

//++
// Details: The invoker requires this function. The command does work in this
//          function. The command is likely to communicate with the LLDB
//          SBDebugger in here.
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakWatch::Execute() {
  CMICMDBASE_GETOPTION(pArgAccess, OptionShort,
                       m_constStrArgNamedAccessWatchpoint);
  CMICMDBASE_GETOPTION(pArgRead, OptionShort, m_constStrArgNamedReadWatchpoint);
  CMICMDBASE_GETOPTION(pArgExpr, Text, m_constStrArgNamedExpr);

  // Ask LLDB for the target to check if we have valid or dummy one.
  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());
  auto sbTarget = rSessionInfo.GetTarget();
  auto sbProcess = rSessionInfo.GetProcess();
  auto sbThread = sbProcess.GetSelectedThread();
  auto sbFrame = sbThread.GetSelectedFrame();

  if (!sbFrame) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_INVALID_FRAME),
                                   m_cmdData.strMiCmd.c_str()));
    return MIstatus::failure;
  }

  lldb::addr_t address;
  size_t size;

  bool isVariable = true;
  auto expression = pArgExpr->GetValue();
  if (!FindLocalVariableAddress(sbTarget, sbFrame, expression, address, size) &&
      !FindGlobalVariableAddress(sbTarget, sbFrame, expression, address,
                                 size)) {
    isVariable = false;
    if (!FindAddressByExpressionEvaluation(sbTarget, sbFrame, expression,
                                           address, size)) {
      SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_FIND_EXPR_ADDRESS),
                                     m_cmdData.strMiCmd.c_str(),
                                     expression.c_str()));
      return MIstatus::failure;
    }
  }

  bool read = pArgAccess->GetFound() || pArgRead->GetFound();
  bool write = !pArgRead->GetFound();

  lldb::SBError sbError;
  m_watchpoint = sbTarget.WatchAddress(address, size, read, write, sbError);

  if (!m_watchpoint) {
    const char *type = "write";
    if (pArgAccess->GetFound())
      type = "access";
    else if (pArgRead->GetFound())
      type = "read";

    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_CREATE_WATCHPOINT), m_cmdData.strMiCmd.c_str(), type,
        static_cast<uint64_t>(address), static_cast<uint64_t>(size)));
    return MIstatus::failure;
  }

  if (!rSessionInfo.GetStoppointInfo(m_watchpoint, m_stoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_WATCHPOINT_STOPPOINT_INFO_CREATE),
        m_cmdData.strMiCmd.c_str(),
        static_cast<uint64_t>(m_watchpoint.GetID())));
    return MIstatus::failure;
  }

  m_stoppointInfo.m_bDisp = false;
  m_stoppointInfo.m_bEnabled = m_watchpoint.IsEnabled();
  m_stoppointInfo.m_bHaveArgOptionThreadGrp = false;
  m_stoppointInfo.m_nTimes = m_watchpoint.GetHitCount();
  m_stoppointInfo.m_watchpointVariable = isVariable;
  m_stoppointInfo.m_watchpointExpr = expression;
  m_stoppointInfo.m_watchpointRead = read;
  m_stoppointInfo.m_watchpointWrite = write;
  m_stoppointInfo.m_nIgnore = m_watchpoint.GetIgnoreCount();
  m_stoppointInfo.m_bPending = false;
  m_stoppointInfo.m_bCondition = m_watchpoint.GetCondition() != nullptr;
  m_stoppointInfo.m_strCondition =
      m_stoppointInfo.m_bCondition ? m_watchpoint.GetCondition() : "";
  m_stoppointInfo.m_bBreakpointThreadId = false;

  if (!rSessionInfo.RecordStoppointInfo(m_stoppointInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPOINT_INFO_SET), m_cmdData.strMiCmd.c_str(),
        static_cast<uint64_t>(m_stoppointInfo.m_nMiId)));
    return MIstatus::failure;
  }

  return MIstatus::success;
}

//++
// Details: The invoker requires this function. The command prepares a MI Record
//          Result for the work carried out in the Execute().
// Type:    Overridden.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdBreakWatch::Acknowledge() {
  assert(m_watchpoint.IsValid());

  CMICmnMIValueResult miValueResult;
  CMICmnLLDBDebugSessionInfo::Instance().MIResponseFormWatchpointInfo(
      m_stoppointInfo, miValueResult);

  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
      miValueResult);
  m_miResultRecord = miRecordResult;

  return MIstatus::success;
}

//++
// Details: Required by the CMICmdFactory when registering *this command. The
//          factory calls this function to create an instance of *this command.
// Type:    Static method.
// Args:    None.
// Return:  CMICmdBase * - Pointer to a new command.
// Throws:  None.
//--
CMICmdBase *CMICmdCmdBreakWatch::CreateSelf() {
  return new CMICmdCmdBreakWatch();
}
