//===-- MICmdCmdBreak.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Overview:    CMICmdCmdBreakInsert            interface.
//              CMICmdCmdBreakDelete            interface.
//              CMICmdCmdBreakDisable           interface.
//              CMICmdCmdBreakEnable            interface.
//              CMICmdCmdBreakAfter             interface.
//              CMICmdCmdBreakCondition         interface.
//
//              To implement new MI commands derive a new command class from the
//              command base
//              class. To enable the new command for interpretation add the new
//              command class
//              to the command factory. The files of relevance are:
//                  MICmdCommands.cpp
//                  MICmdBase.h / .cpp
//                  MICmdCmd.h / .cpp
//              For an introduction to adding a new command see
//              CMICmdCmdSupportInfoMiCmdQuery
//              command class as an example.

#pragma once

// Third party headers:
#include "lldb/API/SBBreakpoint.h"
#include "lldb/API/SBWatchpoint.h"

// In-house headers:
#include "MICmdBase.h"
#include "MICmnLLDBDebugSessionInfo.h"

//++
//============================================================================
// Details: MI command class. MI commands derived from the command base class.
//          *this class implements MI command "break-insert".
//          This command does not follow the MI documentation exactly.
//--
class CMICmdCmdBreakInsert : public CMICmdBase {
  // Statics:
public:
  // Required by the CMICmdFactory when registering *this command
  static CMICmdBase *CreateSelf();

  // Methods:
public:
  /* ctor */ CMICmdCmdBreakInsert();

  // Overridden:
public:
  // From CMICmdInvoker::ICmd
  bool Execute() override;
  bool Acknowledge() override;
  bool ParseArgs() override;
  // From CMICmnBase
  /* dtor */ ~CMICmdCmdBreakInsert() override;

  // Enumerations:
private:
  //++ ===================================================================
  // Details: The type of break point give in the MI command text.
  //--
  enum BreakPoint_e {
    eBreakPoint_Invalid = 0,
    eBreakPoint_ByFileLine,
    eBreakPoint_ByFileFn,
    eBreakPoint_ByName,
    eBreakPoint_ByAddress,
    eBreakPoint_count,
    eBreakPoint_NotDefineYet
  };

  // Attributes:
private:
  bool m_bBreakpointIsTemp;
  bool m_bHaveArgOptionThreadGrp;
  CMIUtilString m_brkName;
  CMIUtilString m_strArgOptionThreadGrp;
  lldb::SBBreakpoint m_breakpoint;
  bool m_bBreakpointIsPending;
  MIuint m_nBreakpointIgnoreCount;
  bool m_bBreakpointEnabled;
  bool m_bBreakpointCondition;
  CMIUtilString m_breakpointCondition;
  bool m_bBreakpointThreadId;
  MIuint m_nBreakpointThreadId;
  const CMIUtilString m_constStrArgNamedTempBreakpoint;
  const CMIUtilString
      m_constStrArgNamedHWBreakpoint; // Not handled by *this command
  const CMIUtilString m_constStrArgNamedPendinfBreakpoint;
  const CMIUtilString m_constStrArgNamedDisableBreakpoint;
  const CMIUtilString m_constStrArgNamedTracePt; // Not handled by *this command
  const CMIUtilString m_constStrArgNamedConditionalBreakpoint;
  const CMIUtilString m_constStrArgNamedInoreCnt;
  const CMIUtilString m_constStrArgNamedRestrictBreakpointToThreadId;
  const CMIUtilString m_constStrArgNamedLocation;
};

//++
//============================================================================
// Details: MI command class. MI commands derived from the command base class.
//          *this class implements MI command "break-delete".
//--
class CMICmdCmdBreakDelete : public CMICmdBase {
  // Statics:
public:
  // Required by the CMICmdFactory when registering *this command
  static CMICmdBase *CreateSelf();

  // Methods:
public:
  /* ctor */ CMICmdCmdBreakDelete();

  // Overridden:
public:
  // From CMICmdInvoker::ICmd
  bool Execute() override;
  bool Acknowledge() override;
  bool ParseArgs() override;
  // From CMICmnBase
  /* dtor */ ~CMICmdCmdBreakDelete() override;

  // Attributes:
private:
  const CMIUtilString m_constStrArgNamedBreakpoint;
};

//++
//============================================================================
// Details: MI command class. MI commands derived from the command base class.
//          *this class implements MI command "break-disable".
//--
class CMICmdCmdBreakDisable : public CMICmdBase {
  // Statics:
public:
  // Required by the CMICmdFactory when registering *this command
  static CMICmdBase *CreateSelf();

  // Methods:
public:
  /* ctor */ CMICmdCmdBreakDisable();

  // Overridden:
public:
  // From CMICmdInvoker::ICmd
  bool Execute() override;
  bool Acknowledge() override;
  bool ParseArgs() override;
  // From CMICmnBase
  /* dtor */ ~CMICmdCmdBreakDisable() override;

  // Attributes:
private:
  const CMIUtilString m_constStrArgNamedBreakpoint;
  bool m_bBreakpointDisabledOk;
  MIuint m_nMiStoppointId;
};

//++
//============================================================================
// Details: MI command class. MI commands derived from the command base class.
//          *this class implements MI command "break-enable".
//--
class CMICmdCmdBreakEnable : public CMICmdBase {
  // Statics:
public:
  // Required by the CMICmdFactory when registering *this command
  static CMICmdBase *CreateSelf();

  // Methods:
public:
  /* ctor */ CMICmdCmdBreakEnable();

  // Overridden:
public:
  // From CMICmdInvoker::ICmd
  bool Execute() override;
  bool Acknowledge() override;
  bool ParseArgs() override;
  // From CMICmnBase
  /* dtor */ ~CMICmdCmdBreakEnable() override;

  // Attributes:
private:
  const CMIUtilString m_constStrArgNamedBreakpoint;
  bool m_bBreakpointEnabledOk;
  MIuint m_nMiStoppointId;
};

//++
//============================================================================
// Details: MI command class. MI commands derived from the command base class.
//          *this class implements MI command "break-after".
//--
class CMICmdCmdBreakAfter : public CMICmdBase {
  // Statics:
public:
  // Required by the CMICmdFactory when registering *this command
  static CMICmdBase *CreateSelf();

  // Methods:
public:
  /* ctor */ CMICmdCmdBreakAfter();

  // Overridden:
public:
  // From CMICmdInvoker::ICmd
  bool Execute() override;
  bool Acknowledge() override;
  bool ParseArgs() override;
  // From CMICmnBase
  /* dtor */ ~CMICmdCmdBreakAfter() override;

  // Methods:
private:
  bool UpdateStoppointInfo(CMICmnLLDBDebugSessionInfo &rSessionInfo);
  template <class T>
  bool SetIgnoreCount(CMICmnLLDBDebugSessionInfo &rSessionInfo,
                      T &vrStoppoint) {
    if (!vrStoppoint.IsValid()) {
      const CMIUtilString strBreakpointId(CMIUtilString::Format(
          "%" PRIu64, static_cast<uint64_t>(m_nMiStoppointId)));
      SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID),
                                     m_cmdData.strMiCmd.c_str(),
                                     strBreakpointId.c_str()));
      return MIstatus::failure;
    }

    vrStoppoint.SetIgnoreCount(m_nBreakpointCount);

    return UpdateStoppointInfo(rSessionInfo);
  }

  // Attributes:
private:
  const CMIUtilString m_constStrArgNamedNumber;
  const CMIUtilString m_constStrArgNamedCount;
  MIuint m_nMiStoppointId;
  MIuint m_nBreakpointCount;
};

//++
//============================================================================
// Details: MI command class. MI commands derived from the command base class.
//          *this class implements MI command "break-condition".
//--
class CMICmdCmdBreakCondition : public CMICmdBase {
  // Statics:
public:
  // Required by the CMICmdFactory when registering *this command
  static CMICmdBase *CreateSelf();

  // Methods:
public:
  /* ctor */ CMICmdCmdBreakCondition();

  // Overridden:
public:
  // From CMICmdInvoker::ICmd
  bool Execute() override;
  bool Acknowledge() override;
  bool ParseArgs() override;
  // From CMICmnBase
  /* dtor */ ~CMICmdCmdBreakCondition() override;

  // Methods:
private:
  CMIUtilString GetRestOfExpressionNotSurroundedInQuotes();
  bool UpdateStoppointInfo(CMICmnLLDBDebugSessionInfo &rSessionInfo);
  template <class T>
  bool SetCondition(CMICmnLLDBDebugSessionInfo &rSessionInfo, T &vrStoppoint) {
    if (!vrStoppoint.IsValid()) {
      const CMIUtilString strBreakpointId(CMIUtilString::Format(
          "%" PRIu64, static_cast<uint64_t>(m_nMiStoppointId)));
      SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_STOPPOINT_INVALID),
                                     m_cmdData.strMiCmd.c_str(),
                                     strBreakpointId.c_str()));
      return MIstatus::failure;
    }

    vrStoppoint.SetCondition(m_strBreakpointExpr.c_str());

    return UpdateStoppointInfo(rSessionInfo);
  }

  // Attributes:
private:
  const CMIUtilString m_constStrArgNamedNumber;
  const CMIUtilString m_constStrArgNamedExpr;
  const CMIUtilString m_constStrArgNamedExprNoQuotes; // Not specified in MI
                                                      // spec, we need to handle
                                                      // expressions not
                                                      // surrounded by quotes
  MIuint m_nMiStoppointId;
  CMIUtilString m_strBreakpointExpr;
};

//++
//============================================================================
// Details: MI command class. MI commands derived from the command base class.
//          *this class implements MI command "break-watch".
//--
class CMICmdCmdBreakWatch : public CMICmdBase {
  // Statics:
public:
  // Required by the CMICmdFactory when registering *this command
  static CMICmdBase *CreateSelf();

  // Methods:
public:
  /* ctor */ CMICmdCmdBreakWatch();

  // Overridden:
public:
  // From CMICmdInvoker::ICmd
  bool Execute() override;
  bool Acknowledge() override;
  bool ParseArgs() override;
  // From CMICmnBase
  /* dtor */ ~CMICmdCmdBreakWatch() override = default;

  // Methods:
private:
  // Attributes:
private:
  const CMIUtilString m_constStrArgNamedAccessWatchpoint;
  const CMIUtilString m_constStrArgNamedReadWatchpoint;
  const CMIUtilString m_constStrArgNamedExpr;

  CMICmnLLDBDebugSessionInfo::SStoppointInfo m_stoppointInfo;
  lldb::SBWatchpoint m_watchpoint;
};
