//===-- MICmnLLDBDebugSessionInfo.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

// Third party headers:
#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBListener.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBTarget.h"
#include <map>
#include <type_traits>
#include <vector>

// In-house headers:
#include "MICmnBase.h"
#include "MICmnLLDBDebugSessionInfoVarObj.h"
#include "MICmnMIValueTuple.h"
#include "MIUtilMapIdToVariant.h"
#include "MIUtilSingletonBase.h"
#include "MIUtilThreadBaseStd.h"

// Declarations:
class CMICmnLLDBDebugger;
struct SMICmdData;
class CMICmnMIValueTuple;
class CMICmnMIValueList;

//++
//============================================================================
// Details: MI debug session object that holds debugging information between
//          instances of MI commands executing their work and producing MI
//          result records. Information/data is set by one or many commands then
//          retrieved by the same or other subsequent commands.
//          It primarily holds LLDB type objects.
//          A singleton class.
//--
class CMICmnLLDBDebugSessionInfo
    : public CMICmnBase,
      public MI::ISingleton<CMICmnLLDBDebugSessionInfo> {
  friend class MI::ISingleton<CMICmnLLDBDebugSessionInfo>;

  // Enumerations:
public:
  //++ ===================================================================
  // Details: The type of variable used by MIResponseFormVariableInfo family
  // functions.
  //--
  enum VariableType_e {
    eVariableType_InScope = (1u << 0),  // In scope only.
    eVariableType_Statics = (1u << 1),  // Statics.
    eVariableType_Locals = (1u << 2),   // Locals.
    eVariableType_Arguments = (1u << 3) // Arguments.
  };

  //++ ===================================================================
  // Details: The type of stoppoint.
  //--
  enum StoppointType_e {
    eStoppointType_Breakpoint,
    eStoppointType_Watchpoint,
  };

  //++ ===================================================================
  // Details: Determine the information that should be shown by using
  // MIResponseFormVariableInfo family functions.
  //--
  enum VariableInfoFormat_e {
    eVariableInfoFormat_NoValues = 0,
    eVariableInfoFormat_AllValues = 1,
    eVariableInfoFormat_SimpleValues = 2
  };

  //++ ===================================================================
  // Details: Determine the information that should be shown by using
  // MIResponseFormThreadInfo family functions.
  //--
  enum ThreadInfoFormat_e {
    eThreadInfoFormat_NoFrames,
    eThreadInfoFormat_AllFrames
  };

  //++ ===================================================================
  // Details: Determine the information that should be shown by using
  // MIResponseFormFrameInfo family functions.
  //--
  enum FrameInfoFormat_e {
    eFrameInfoFormat_NoArguments,
    eFrameInfoFormat_AllArguments,
    eFrameInfoFormat_AllArgumentsInSimpleForm
  };

  // Structs:
public:
  //++
  //============================================================================
  // Details: Stop point information object. Used to easily pass information
  //          about a break around and record stop point information to be
  //          recalled by other commands or LLDB event handling functions.
  //--
  struct SStoppointInfo {
    SStoppointInfo()
        : m_nLldbId(0), m_nMiId(0), m_bDisp(false), m_bEnabled(false),
          m_addr(0), m_nLine(0), m_bHaveArgOptionThreadGrp(false), m_nTimes(0),
          m_watchpointVariable(false), m_watchpointRead(false),
          m_watchpointWrite(false), m_bPending(false), m_nIgnore(0),
          m_bCondition(false), m_bBreakpointThreadId(false),
          m_nBreakpointThreadId(0) {}

    MIuint m_nLldbId;         // LLDB break or watch point ID.
    MIuint m_nMiId;           // Emulated GDB-MI break point ID.
    StoppointType_e m_eType;  // Stop point type.
    bool m_bDisp;             // True = "del", false = "keep".
    bool m_bEnabled;          // True = enabled, false = disabled break point.
    lldb::addr_t m_addr;      // Address number.
    CMIUtilString m_fnName;   // Function name.
    CMIUtilString m_fileName; // File name text.
    CMIUtilString m_path;     // Full file name and path text.
    MIuint m_nLine;           // File line number.
    bool m_bHaveArgOptionThreadGrp; // True = include MI field, false = do not
                                    // include "thread-groups".
    CMIUtilString m_strOptThrdGrp;  // Thread group number.
    MIuint m_nTimes;                // The count of the breakpoint existence.
    CMIUtilString m_strOrigLoc;     // The name of the break point.
    bool m_watchpointVariable;      // Whether the watchpoint is set on var.
    CMIUtilString m_watchpointExpr; // The expression of the watch point.
    bool m_watchpointRead;          // Whether the wpt is triggered on read.
    bool m_watchpointWrite;         // Whether the wpt is triggered on write.
    bool m_bPending;  // True = the breakpoint has not been established yet,
                      // false = location found
    MIuint m_nIgnore; // The number of time the breakpoint is run over before it
                      // is stopped on a hit
    bool m_bCondition; // True = break point is conditional, use condition
                       // expression, false = no condition
    CMIUtilString m_strCondition; // Break point condition expression
    bool m_bBreakpointThreadId; // True = break point is specified to work with
                                // a specific thread, false = no specified
                                // thread given
    MIuint m_nBreakpointThreadId; // Restrict the breakpoint to the specified
                                  // thread-id
  };

  // Typedefs:
public:
  typedef std::vector<uint32_t> VecActiveThreadId_t;

  // Methods:
public:
  bool Initialize() override;
  bool Shutdown() override;

  // Variant type data which can be assigned and retrieved across all command
  // instances
  template <typename T>
  bool SharedDataAdd(const CMIUtilString &vKey, const T &vData);
  template <typename T>
  bool SharedDataRetrieve(const CMIUtilString &vKey, T &vwData);
  void SharedDataDestroy();

  //  Common command required functionality
  bool AccessPath(const CMIUtilString &vPath, bool &vwbYesAccessible);
  bool ResolvePath(const SMICmdData &vCmdData, const CMIUtilString &vPath,
                   CMIUtilString &vwrResolvedPath);
  bool ResolvePath(const CMIUtilString &vstrUnknown,
                   CMIUtilString &vwrResolvedPath);
  bool MIResponseFormFrameInfo(const lldb::SBThread &vrThread,
                               const MIuint vnLevel,
                               const FrameInfoFormat_e veFrameInfoFormat,
                               CMICmnMIValueTuple &vwrMiValueTuple);
  bool MIResponseFormThreadInfo(const SMICmdData &vCmdData,
                                const lldb::SBThread &vrThread,
                                const ThreadInfoFormat_e veThreadInfoFormat,
                                CMICmnMIValueTuple &vwrMIValueTuple);
  bool MIResponseFormVariableInfo(const lldb::SBFrame &vrFrame,
                                  const MIuint vMaskVarTypes,
                                  const VariableInfoFormat_e veVarInfoFormat,
                                  CMICmnMIValueList &vwrMiValueList,
                                  const MIuint vnMaxDepth = 10,
                                  const bool vbMarkArgs = false);
  void MIResponseFormStoppointFrameInfo(const SStoppointInfo &vrStoppointInfo,
                                        CMICmnMIValueTuple &vwrMiValueTuple);
  bool MIResponseFormBreakpointInfo(const SStoppointInfo &vrStoppointInfo,
                                    CMICmnMIValueTuple &vwrMiValueTuple);
  void MIResponseFormWatchpointInfo(const SStoppointInfo &vrStoppointInfo,
                                    CMICmnMIValueResult &vwrMiValueResult);
  template <class T, class = std::enable_if_t<
                         std::is_same<T, lldb::SBBreakpoint>::value ||
                         std::is_same<T, lldb::SBWatchpoint>::value>>
  bool GetStoppointInfo(const T &vrStoppoint, SStoppointInfo &vrwStoppointInfo);
  bool RecordStoppointInfo(const SStoppointInfo &vrStoppointInfo);
  bool RecordStoppointInfoGet(const MIuint vnMiStoppointId,
                              SStoppointInfo &vrwStoppointInfo) const;
  bool RecordStoppointInfoDelete(const MIuint vnMiStoppointId);
  MIuint GetOrCreateMiStoppointId(const MIuint vnLldbStoppointId,
                                  const StoppointType_e veStoppointType);
  bool RemoveLldbToMiStoppointIdMapping(const MIuint vnLldbStoppointId,
                                        const StoppointType_e veStoppointType);
  CMIUtilThreadMutex &GetSessionMutex() { return m_sessionMutex; }
  lldb::SBDebugger &GetDebugger() const;
  lldb::SBListener &GetListener() const;
  lldb::SBTarget GetTarget() const;
  lldb::SBProcess GetProcess() const;

  void SetCreateTty(bool val);
  bool GetCreateTty() const;

  // Attributes:
public:
  // The following are available to all command instances
  const MIuint m_nBreakpointCntMax;
  VecActiveThreadId_t m_vecActiveThreadId;
  lldb::tid_t m_currentSelectedThread;

  // These are keys that can be used to access the shared data map
  // Note: This list is expected to grow and will be moved and abstracted in the
  // future.
  const CMIUtilString m_constStrSharedDataKeyWkDir;
  const CMIUtilString m_constStrSharedDataSolibPath;
  const CMIUtilString m_constStrPrintCharArrayAsString;
  const CMIUtilString m_constStrPrintExpandAggregates;
  const CMIUtilString m_constStrPrintAggregateFieldNames;

  // Typedefs:
private:
  typedef std::vector<CMICmnLLDBDebugSessionInfoVarObj> VecVarObj_t;
  typedef std::map<MIuint, SStoppointInfo> MapMiStoppointIdToStoppointInfo_t;
  typedef std::pair<MIuint, SStoppointInfo>
      MapPairMiStoppointIdToStoppointInfo_t;
  typedef std::map<std::pair<MIuint, StoppointType_e>, MIuint>
      MapLldbStoppointIdToMiStoppointId_t;

  // Methods:
private:
  /* ctor */ CMICmnLLDBDebugSessionInfo();
  /* ctor */ CMICmnLLDBDebugSessionInfo(const CMICmnLLDBDebugSessionInfo &);
  void operator=(const CMICmnLLDBDebugSessionInfo &);
  //
  bool GetVariableInfo(const lldb::SBValue &vrValue, const bool vbInSimpleForm,
                       CMIUtilString &vwrStrValue);
  bool GetFrameInfo(const lldb::SBFrame &vrFrame, lldb::addr_t &vwPc,
                    CMIUtilString &vwFnName, CMIUtilString &vwFileName,
                    CMIUtilString &vwPath, MIuint &vwnLine);
  bool GetThreadFrames(const SMICmdData &vCmdData, const MIuint vThreadIdx,
                       const FrameInfoFormat_e veFrameInfoFormat,
                       CMIUtilString &vwrThreadFrames);
  bool
  MIResponseForVariableInfoInternal(const VariableInfoFormat_e veVarInfoFormat,
                                    CMICmnMIValueList &vwrMiValueList,
                                    const lldb::SBValueList &vwrSBValueList,
                                    const MIuint vnMaxDepth,
                                    const bool vbIsArgs, const bool vbMarkArgs);

  // Overridden:
private:
  // From CMICmnBase
  /* dtor */ ~CMICmnLLDBDebugSessionInfo() override;

  // Attributes:
private:
  CMIUtilMapIdToVariant m_mapIdToSessionData; // Hold and retrieve key to value
                                              // data available across all
                                              // commands
  VecVarObj_t m_vecVarObj; // Vector of session variable objects
  MapMiStoppointIdToStoppointInfo_t m_mapMiStoppointIdToStoppointInfo;
  CMIUtilThreadMutex m_sessionMutex;
  MapLldbStoppointIdToMiStoppointId_t m_mapLldbStoppointIdToMiStoppointId;
  MIuint m_nNextMiStoppointId;
  std::mutex m_miStoppointIdsMutex;

  bool m_bCreateTty; // Created inferiors should launch with new TTYs
};

//++
// Details: Command instances can create and share data between other instances
// of commands.
//          This function adds new data to the shared data. Using the same ID
//          more than
//          once replaces any previous matching data keys.
// Type:    Template method.
// Args:    T       - The type of the object to be stored.
//          vKey    - (R) A non empty unique data key to retrieve the data by.
//          vData   - (R) Data to be added to the share.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
template <typename T>
bool CMICmnLLDBDebugSessionInfo::SharedDataAdd(const CMIUtilString &vKey,
                                               const T &vData) {
  if (!m_mapIdToSessionData.Add<T>(vKey, vData)) {
    SetErrorDescription(m_mapIdToSessionData.GetErrorDescription());
    return MIstatus::failure;
  }

  return MIstatus::success;
}

//++
// Details: Command instances can create and share data between other instances
// of commands.
//          This function retrieves data from the shared data container.
// Type:    Method.
// Args:    T     - The type of the object being retrieved.
//          vKey  - (R) A non empty unique data key to retrieve the data by.
//          vData - (W) The data.
// Return:  bool  - True = data found, false = data not found or an error
// occurred trying to fetch.
// Throws:  None.
//--
template <typename T>
bool CMICmnLLDBDebugSessionInfo::SharedDataRetrieve(const CMIUtilString &vKey,
                                                    T &vwData) {
  bool bDataFound = false;

  if (!m_mapIdToSessionData.Get<T>(vKey, vwData, bDataFound)) {
    SetErrorDescription(m_mapIdToSessionData.GetErrorDescription());
    return MIstatus::failure;
  }

  return bDataFound;
}
