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
#include "lldb/API/SBBreakpointLocation.h"

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
    : m_bBrkPtIsTemp(false), m_bBrkPtIsPending(false), m_nBrkPtIgnoreCount(0),
      m_bBrkPtEnabled(false), m_bBrkPtCondition(false), m_bBrkPtThreadId(false),
      m_nBrkPtThreadId(0), m_constStrArgNamedTempBrkPt("t"),
      m_constStrArgNamedHWBrkPt("h"), m_constStrArgNamedPendinfBrkPt("f"),
      m_constStrArgNamedDisableBrkPt("d"), m_constStrArgNamedTracePt("a"),
      m_constStrArgNamedConditionalBrkPt("c"), m_constStrArgNamedInoreCnt("i"),
      m_constStrArgNamedRestrictBrkPtToThreadId("p"),
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
  m_setCmdArgs.Add(
      new CMICmdArgValOptionShort(m_constStrArgNamedTempBrkPt, false, true));
  // Not implemented m_setCmdArgs.Add(new CMICmdArgValOptionShort(
  // m_constStrArgNamedHWBrkPt, false, false));
  m_setCmdArgs.Add(
      new CMICmdArgValOptionShort(m_constStrArgNamedPendinfBrkPt, false, true));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(m_constStrArgNamedDisableBrkPt,
                                               false, false));
  // Not implemented m_setCmdArgs.Add(new CMICmdArgValOptionShort(
  // m_constStrArgNamedTracePt, false, false));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(
      m_constStrArgNamedConditionalBrkPt, false, true,
      CMICmdArgValListBase::eArgValType_StringQuoted, 1));
  m_setCmdArgs.Add(
      new CMICmdArgValOptionShort(m_constStrArgNamedInoreCnt, false, true,
                                  CMICmdArgValListBase::eArgValType_Number, 1));
  m_setCmdArgs.Add(new CMICmdArgValOptionShort(
      m_constStrArgNamedRestrictBrkPtToThreadId, false, true,
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
  CMICMDBASE_GETOPTION(pArgTempBrkPt, OptionShort, m_constStrArgNamedTempBrkPt);
  CMICMDBASE_GETOPTION(pArgThreadGroup, OptionLong, m_constStrArgThreadGroup);
  CMICMDBASE_GETOPTION(pArgLocation, String, m_constStrArgNamedLocation);
  CMICMDBASE_GETOPTION(pArgIgnoreCnt, OptionShort, m_constStrArgNamedInoreCnt);
  CMICMDBASE_GETOPTION(pArgPendingBrkPt, OptionShort,
                       m_constStrArgNamedPendinfBrkPt);
  CMICMDBASE_GETOPTION(pArgDisableBrkPt, OptionShort,
                       m_constStrArgNamedDisableBrkPt);
  CMICMDBASE_GETOPTION(pArgConditionalBrkPt, OptionShort,
                       m_constStrArgNamedConditionalBrkPt);
  CMICMDBASE_GETOPTION(pArgRestrictBrkPtToThreadId, OptionShort,
                       m_constStrArgNamedRestrictBrkPtToThreadId);

  // Ask LLDB for the target to check if we have valid or dummy one.
  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());
  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();

  m_bBrkPtEnabled = !pArgDisableBrkPt->GetFound();
  m_bBrkPtIsTemp = pArgTempBrkPt->GetFound();
  m_bHaveArgOptionThreadGrp = pArgThreadGroup->GetFound();
  if (m_bHaveArgOptionThreadGrp) {
    MIuint nThreadGrp = 0;
    pArgThreadGroup->GetExpectedOption<CMICmdArgValThreadGrp, MIuint>(
        nThreadGrp);
    m_strArgOptionThreadGrp = CMIUtilString::Format("i%d", nThreadGrp);
  }

  if (sbTarget == rSessionInfo.GetDebugger().GetDummyTarget())
    m_bBrkPtIsPending = true;
  else {
    m_bBrkPtIsPending = pArgPendingBrkPt->GetFound();
    if (!m_bBrkPtIsPending) {
      CMIUtilString pending;
      if (m_rLLDBDebugSessionInfo.SharedDataRetrieve("breakpoint.pending",
                                                     pending)) {
        m_bBrkPtIsPending = pending == "on";
      }
    }
  }

  if (pArgLocation->GetFound())
    m_brkName = pArgLocation->GetValue();
  else if (m_bBrkPtIsPending) {
    pArgPendingBrkPt->GetExpectedOption<CMICmdArgValString, CMIUtilString>(
        m_brkName);
  }
  if (pArgIgnoreCnt->GetFound()) {
    pArgIgnoreCnt->GetExpectedOption<CMICmdArgValNumber, MIuint>(
        m_nBrkPtIgnoreCount);
  }
  m_bBrkPtCondition = pArgConditionalBrkPt->GetFound();
  if (m_bBrkPtCondition) {
    pArgConditionalBrkPt->GetExpectedOption<CMICmdArgValString, CMIUtilString>(
        m_brkPtCondition);
  }
  m_bBrkPtThreadId = pArgRestrictBrkPtToThreadId->GetFound();
  if (m_bBrkPtCondition) {
    pArgRestrictBrkPtToThreadId->GetExpectedOption<CMICmdArgValNumber, MIuint>(
        m_nBrkPtThreadId);
  }

  // Determine if break on a file line or at a function
  BreakPoint_e eBrkPtType = eBreakPoint_NotDefineYet;
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
      eBrkPtType = eBreakPoint_ByName;
    else {
      MIint64 nValue = 0;
      if (rStrLineOrFn.ExtractNumber(nValue)) {
        nFileLine = static_cast<MIuint>(nValue);
        eBrkPtType = eBreakPoint_ByFileLine;
      } else {
        strFileFn = rStrLineOrFn;
        eBrkPtType = eBreakPoint_ByFileFn;
      }
    }
  }

  // Determine if break defined as an address
  lldb::addr_t nAddress = 0;
  if (eBrkPtType == eBreakPoint_NotDefineYet) {
    if (!m_brkName.empty() && m_brkName[0] == '*') {
      MIint64 nValue = 0;
      if (CMIUtilString(m_brkName.substr(1)).ExtractNumber(nValue)) {
        nAddress = static_cast<lldb::addr_t>(nValue);
        eBrkPtType = eBreakPoint_ByAddress;
      }
    }
  }

  // Break defined as an function
  if (eBrkPtType == eBreakPoint_NotDefineYet) {
    eBrkPtType = eBreakPoint_ByName;
  }

  // Ask LLDB to create a breakpoint
  bool bOk = MIstatus::success;
  switch (eBrkPtType) {
  case eBreakPoint_ByAddress:
    m_brkPt = sbTarget.BreakpointCreateByAddress(nAddress);
    break;
  case eBreakPoint_ByFileFn: {
    lldb::SBFileSpecList module; // search in all modules
    lldb::SBFileSpecList compUnit;
    compUnit.Append(lldb::SBFileSpec(fileName.c_str()));
    m_brkPt =
        sbTarget.BreakpointCreateByName(strFileFn.c_str(), module, compUnit);
    break;
  }
  case eBreakPoint_ByFileLine:
    m_brkPt = sbTarget.BreakpointCreateByLocation(fileName.c_str(), nFileLine);
    break;
  case eBreakPoint_ByName:
    m_brkPt = sbTarget.BreakpointCreateByName(m_brkName.c_str(), nullptr);
    break;
  case eBreakPoint_count:
  case eBreakPoint_NotDefineYet:
  case eBreakPoint_Invalid:
    bOk = MIstatus::failure;
    break;
  }

  if (bOk) {
    if (!m_bBrkPtIsPending && (m_brkPt.GetNumLocations() == 0)) {
      sbTarget.BreakpointDelete(m_brkPt.GetID());
      SetError(
          CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_BRKPT_LOCATION_NOT_FOUND),
                                m_cmdData.strMiCmd.c_str(), m_brkName.c_str()));
      return MIstatus::failure;
    }

    m_brkPt.SetEnabled(m_bBrkPtEnabled);
    m_brkPt.SetIgnoreCount(m_nBrkPtIgnoreCount);
    m_brkPt.SetOneShot(m_bBrkPtIsTemp);
    if (m_bBrkPtCondition)
      m_brkPt.SetCondition(m_brkPtCondition.c_str());
    if (m_bBrkPtThreadId)
      m_brkPt.SetThreadID(m_nBrkPtThreadId);
  }

  // CODETAG_LLDB_BREAKPOINT_CREATION
  // This is in the main thread
  // Record break point information to be by LLDB event handler function
  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.GetStopPtInfo(m_brkPt, sStopPtInfo))
    return MIstatus::failure;

  sStopPtInfo.m_bDisp = m_bBrkPtIsTemp;
  sStopPtInfo.m_bEnabled = m_bBrkPtEnabled;
  sStopPtInfo.m_bHaveArgOptionThreadGrp = m_bHaveArgOptionThreadGrp;
  sStopPtInfo.m_strOptThrdGrp = m_strArgOptionThreadGrp;
  sStopPtInfo.m_nTimes = m_brkPt.GetHitCount();
  sStopPtInfo.m_strOrigLoc = m_brkName;
  sStopPtInfo.m_nIgnore = m_nBrkPtIgnoreCount;
  sStopPtInfo.m_bPending = m_bBrkPtIsPending;
  sStopPtInfo.m_bCondition = m_bBrkPtCondition;
  sStopPtInfo.m_strCondition = m_brkPtCondition;
  sStopPtInfo.m_bBrkPtThreadId = m_bBrkPtThreadId;
  sStopPtInfo.m_nBrkPtThreadId = m_nBrkPtThreadId;

  bOk = bOk && rSessionInfo.RecordStopPtInfo(sStopPtInfo);
  if (!bOk) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_brkName.c_str()));
    return MIstatus::failure;
  }

  // CODETAG_LLDB_STOPPT_ID_MAX
  if (sStopPtInfo.m_nMiId > rSessionInfo.m_nBrkPointCntMax) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_CNT_EXCEEDED), m_cmdData.strMiCmd.c_str(),
        static_cast<uint64_t>(rSessionInfo.m_nBrkPointCntMax),
        static_cast<uint64_t>(sStopPtInfo.m_nMiId)));
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

  MIuint nMiStopPtId =
      CMICmnLLDBDebugSessionInfo::Instance().GetOrCreateMiStopPtId(
          m_brkPt.GetID(),
          CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint);

  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(nMiStopPtId)));
    return MIstatus::failure;
  }

  // MI print
  // "^done,bkpt={number=\"%d\",type=\"breakpoint\",disp=\"%s\",enabled=\"%c\",addr=\"0x%016"
  // PRIx64
  // "\",func=\"%s\",file=\"%s\",fullname=\"%s/%s\",line=\"%d\",thread-groups=[\"%s\"],times=\"%d\",original-location=\"%s\"}"
  CMICmnMIValueTuple miValueTuple;
  if (!rSessionInfo.MIResponseFormBrkPtInfo(sStopPtInfo, miValueTuple))
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
    : m_constStrArgNamedBrkPt("breakpoint") {
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
      new CMICmdArgValListOfN(m_constStrArgNamedBrkPt, true, true,
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
  CMICMDBASE_GETOPTION(pArgBrkPt, ListOfN, m_constStrArgNamedBrkPt);

  // ATM we only handle one break point ID
  MIuint nMiStopPtId;
  if (!pArgBrkPt->GetExpectedOption<CMICmdArgValNumber, MIuint>(nMiStopPtId)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgNamedBrkPt.c_str()));
    return MIstatus::failure;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(nMiStopPtId)));
    return MIstatus::failure;
  }

  bool bSuccess = false;
  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStopPtInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint)
    bSuccess = sbTarget.BreakpointDelete(
        static_cast<lldb::break_id_t>(sStopPtInfo.m_nLldbId));
  else
    bSuccess = sbTarget.DeleteWatchpoint(
        static_cast<lldb::watch_id_t>(sStopPtInfo.m_nLldbId));

  if (!bSuccess) {
    const CMIUtilString strBrkNum(
        CMIUtilString::Format("%" PRIu64, static_cast<uint64_t>(nMiStopPtId)));
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPT_INVALID),
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
    : m_constStrArgNamedBrkPt("breakpoint"), m_bBrkPtDisabledOk(false),
      m_nMiStopPtId(0) {
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
      new CMICmdArgValListOfN(m_constStrArgNamedBrkPt, true, true,
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
  CMICMDBASE_GETOPTION(pArgBrkPt, ListOfN, m_constStrArgNamedBrkPt);

  // ATM we only handle one break point ID
  if (!pArgBrkPt->GetExpectedOption<CMICmdArgValNumber, MIuint>(
          m_nMiStopPtId)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgNamedBrkPt.c_str()));
    return MIstatus::failure;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(m_nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStopPtId)));
    return MIstatus::failure;
  }

  auto disable = [&m_bBrkPtDisabledOk = m_bBrkPtDisabledOk](auto &vrPt) {
    if (vrPt.IsValid()) {
      m_bBrkPtDisabledOk = true;
      vrPt.SetEnabled(false);
    }
  };

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStopPtInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint brkPt = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStopPtInfo.m_nLldbId));
    disable(brkPt);
  } else {
    lldb::SBWatchpoint watchPt = sbTarget.FindWatchpointByID(
        static_cast<lldb::watch_id_t>(sStopPtInfo.m_nLldbId));
    disable(watchPt);
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
  if (m_bBrkPtDisabledOk) {
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done);
    m_miResultRecord = miRecordResult;
    return MIstatus::success;
  }

  const CMIUtilString strBrkPtId(
      CMIUtilString::Format("%" PRIu64, static_cast<uint64_t>(m_nMiStopPtId)));
  const CMICmnMIValueConst miValueConst(CMIUtilString::Format(
      MIRSRC(IDS_CMD_ERR_STOPPT_INVALID), strBrkPtId.c_str()));
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
    : m_constStrArgNamedBrkPt("breakpoint"), m_bBrkPtEnabledOk(false),
      m_nMiStopPtId(0) {
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
      new CMICmdArgValListOfN(m_constStrArgNamedBrkPt, true, true,
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
  CMICMDBASE_GETOPTION(pArgBrkPt, ListOfN, m_constStrArgNamedBrkPt);

  // ATM we only handle one break point ID
  if (!pArgBrkPt->GetExpectedOption<CMICmdArgValNumber, MIuint>(
          m_nMiStopPtId)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPT_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgNamedBrkPt.c_str()));
    return MIstatus::failure;
  }

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(m_nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStopPtId)));
    return MIstatus::failure;
  }

  auto enable = [&m_bBrkPtEnabledOk = m_bBrkPtEnabledOk](auto &vrPt) {
    if (vrPt.IsValid()) {
      m_bBrkPtEnabledOk = true;
      vrPt.SetEnabled(true);
    }
  };

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStopPtInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint brkPt = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStopPtInfo.m_nLldbId));
    enable(brkPt);
  } else {
    lldb::SBWatchpoint watchPt = sbTarget.FindWatchpointByID(
        static_cast<lldb::watch_id_t>(sStopPtInfo.m_nLldbId));
    enable(watchPt);
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
  if (m_bBrkPtEnabledOk) {
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done);
    m_miResultRecord = miRecordResult;
    return MIstatus::success;
  }

  const CMIUtilString strBrkPtId(
      CMIUtilString::Format("%" PRIu64, static_cast<uint64_t>(m_nMiStopPtId)));
  const CMICmnMIValueConst miValueConst(CMIUtilString::Format(
      MIRSRC(IDS_CMD_ERR_STOPPT_INVALID), strBrkPtId.c_str()));
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
      m_nMiStopPtId(0), m_nBrkPtCount(0) {
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

  m_nMiStopPtId = static_cast<MIuint>(pArgNumber->GetValue());
  m_nBrkPtCount = pArgCount->GetValue();

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(m_nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStopPtId)));
    return MIstatus::failure;
  }

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStopPtInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint brkPt = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStopPtInfo.m_nLldbId));
    return SetIgnoreCount(rSessionInfo, brkPt);
  }

  lldb::SBWatchpoint watchPt = sbTarget.FindWatchpointByID(
      static_cast<lldb::watch_id_t>(sStopPtInfo.m_nLldbId));
  return SetIgnoreCount(rSessionInfo, watchPt);
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
bool CMICmdCmdBreakAfter::UpdateStopPtInfo(
    CMICmnLLDBDebugSessionInfo &rSessionInfo) {
  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(m_nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStopPtId)));
    return MIstatus::failure;
  }
  sStopPtInfo.m_nIgnore = m_nBrkPtCount;
  return rSessionInfo.RecordStopPtInfo(sStopPtInfo);
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
      m_nMiStopPtId(0) {
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

  m_nMiStopPtId = static_cast<MIuint>(pArgNumber->GetValue());
  m_strBrkPtExpr = pArgExpr->GetValue();
  m_strBrkPtExpr += GetRestOfExpressionNotSurroundedInQuotes();

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());

  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(m_nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStopPtId)));
    return MIstatus::failure;
  }

  lldb::SBTarget sbTarget = rSessionInfo.GetTarget();
  if (sStopPtInfo.m_eType ==
      CMICmnLLDBDebugSessionInfo::eStoppointType_Breakpoint) {
    lldb::SBBreakpoint brkPt = sbTarget.FindBreakpointByID(
        static_cast<lldb::break_id_t>(sStopPtInfo.m_nLldbId));
    return SetCondition(rSessionInfo, brkPt);
  }

  lldb::SBWatchpoint watchPt = sbTarget.FindWatchpointByID(
      static_cast<lldb::watch_id_t>(sStopPtInfo.m_nLldbId));
  return SetCondition(rSessionInfo, watchPt);
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
bool CMICmdCmdBreakCondition::UpdateStopPtInfo(
    CMICmnLLDBDebugSessionInfo &rSessionInfo) {
  CMICmnLLDBDebugSessionInfo::SStopPtInfo sStopPtInfo;
  if (!rSessionInfo.RecordStopPtInfoGet(m_nMiStopPtId, sStopPtInfo)) {
    SetError(CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_STOPPT_INFO_OBJ_NOT_FOUND),
        m_cmdData.strMiCmd.c_str(), static_cast<uint64_t>(m_nMiStopPtId)));
    return MIstatus::failure;
  }
  sStopPtInfo.m_strCondition = m_strBrkPtExpr;
  return rSessionInfo.RecordStopPtInfo(sStopPtInfo);
}
