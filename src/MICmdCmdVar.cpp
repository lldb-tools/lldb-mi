//===-- MICmdCmdVar.cpp -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Overview:    CMICmdCmdVarCreate                  implementation.
//              CMICmdCmdVarUpdate                  implementation.
//              CMICmdCmdVarDelete                  implementation.
//              CMICmdCmdVarAssign                  implementation.
//              CMICmdCmdVarSetFormat               implementation.
//              CMICmdCmdVarListChildren            implementation.
//              CMICmdCmdVarEvaluateExpression      implementation.
//              CMICmdCmdVarInfoPathExpression      implementation.
//              CMICmdCmdVarShowAttributes          implementation.

// Third Party Headers:
#include "lldb/API/SBStream.h"
#include "lldb/API/SBThread.h"
#include "lldb/API/SBType.h"

// In-house headers:
#include "MICmdArgValListOfN.h"
#include "MICmdArgValNumber.h"
#include "MICmdArgValOptionLong.h"
#include "MICmdArgValOptionShort.h"
#include "MICmdArgValPrintValues.h"
#include "MICmdArgValString.h"
#include "MICmdArgValText.h"
#include "MICmdArgValThreadGrp.h"
#include "MICmdCmdVar.h"
#include "MICmnLLDBDebugSessionInfo.h"
#include "MICmnLLDBDebugger.h"
#include "MICmnLLDBProxySBValue.h"
#include "MICmnLLDBUtilSBValue.h"
#include "MICmnMIResultRecord.h"
#include "MICmnMIValueConst.h"

#include <algorithm>

namespace {

/// Return a well-formed name for the given variable child.
CMIUtilString GetMemberName(const CMIUtilString &parentName,
                            const CMIUtilString &memberName,
                            MIuint memberIndex) {
  if (memberName.empty())
    return CMIUtilString::Format("%s.$%u", parentName.c_str(), memberIndex);
  return CMIUtilString::Format("%s.%s", parentName.c_str(), memberName.c_str());
}

} // namespace

//++
// Details: CMICmdCmdVarCreate constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarCreate::CMICmdCmdVarCreate()
    : m_nChildren(0), m_nThreadId(0), m_strType("??"), m_bValid(false),
      m_strValue("??"), m_constStrArgName("name"),
      m_constStrArgFrameAddr("frame-addr"),
      m_constStrArgExpression("expression") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-create";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarCreate::CreateSelf;
}

//++
// Details: CMICmdCmdVarCreate destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarCreate::~CMICmdCmdVarCreate() {}

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
bool CMICmdCmdVarCreate::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, false, true));
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgFrameAddr, false, true));
  m_setCmdArgs.Add(new CMICmdArgValText(m_constStrArgExpression, true, true));
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
bool CMICmdCmdVarCreate::Execute() {
  CMICMDBASE_GETOPTION(pArgThread, OptionLong, m_constStrArgThread);
  CMICMDBASE_GETOPTION(pArgFrame, OptionLong, m_constStrArgFrame);
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);
  CMICMDBASE_GETOPTION(pArgFrameAddr, String, m_constStrArgFrameAddr);
  CMICMDBASE_GETOPTION(pArgExpression, Text, m_constStrArgExpression);

  // Retrieve the --thread option's thread ID (only 1)
  MIuint64 nThreadId = UINT64_MAX;
  if (pArgThread->GetFound() &&
      !pArgThread->GetExpectedOption<CMICmdArgValNumber, MIuint64>(nThreadId)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_OPTION_NOT_FOUND),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgThread.c_str()));
    return MIstatus::failure;
  }

  // Retrieve the --frame option's number
  MIuint64 nFrame = UINT64_MAX;
  if (pArgThread->GetFound() &&
      !pArgFrame->GetExpectedOption<CMICmdArgValNumber, MIuint64>(nFrame)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_OPTION_NOT_FOUND),
                                   m_cmdData.strMiCmd.c_str(),
                                   m_constStrArgFrame.c_str()));
    return MIstatus::failure;
  }

  const CMICmdArgValOptionLong::VecArgObjPtr_t &rVecFrameId(
      pArgFrame->GetExpectedOptions());
  CMICmdArgValOptionLong::VecArgObjPtr_t::const_iterator it2 =
      rVecFrameId.begin();
  if (it2 != rVecFrameId.end()) {
    const CMICmdArgValNumber *pOption = static_cast<CMICmdArgValNumber *>(*it2);
    nFrame = pOption->GetValue();
  }

  m_strVarName = "<unnamedvariable>";
  if (pArgName->GetFound()) {
    const CMIUtilString &rArg = pArgName->GetValue();
    const bool bAutoName = (rArg == "-");
    if (bAutoName) {
      m_strVarName = CMIUtilString::Format(
          "var%u", CMICmnLLDBDebugSessionInfoVarObj::VarObjIdGet());
      CMICmnLLDBDebugSessionInfoVarObj::VarObjIdInc();
    } else
      m_strVarName = rArg;
  }

  bool bCurrentFrame = false;
  if (pArgFrameAddr->GetFound()) {
    const CMIUtilString &rStrFrameAddr(pArgFrameAddr->GetValue());
    bCurrentFrame = CMIUtilString::Compare(rStrFrameAddr, "*");
    if (!bCurrentFrame && (nFrame == UINT64_MAX)) {
      // FIXME: *addr isn't implemented. Exit with error if --thread isn't
      // specified.
      SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_OPTION_NOT_FOUND),
                                     m_cmdData.strMiCmd.c_str(),
                                     m_constStrArgFrame.c_str()));
      return MIstatus::failure;
    }
  }

  const CMIUtilString &rStrExpression(pArgExpression->GetValue());
  m_strExpression = rStrExpression;

  CMICmnLLDBDebugSessionInfo &rSessionInfo(
      CMICmnLLDBDebugSessionInfo::Instance());
  lldb::SBProcess sbProcess = rSessionInfo.GetProcess();
  lldb::SBThread thread = (nThreadId != UINT64_MAX)
                              ? sbProcess.GetThreadByIndexID(nThreadId)
                              : sbProcess.GetSelectedThread();
  m_nThreadId = thread.GetIndexID();
  lldb::SBFrame frame = bCurrentFrame ? thread.GetSelectedFrame()
                                      : thread.GetFrameAtIndex(nFrame);
  lldb::SBValue value;
  ValObjKind_ec valueObjKind = ValObjKind_ec::eValObjKind_Other;

  if (rStrExpression[0] == '$') {
    const CMIUtilString rStrRegister(rStrExpression.substr(1));
    value = frame.FindRegister(rStrRegister.c_str());
  } else {
    const bool bArgs = true;
    const bool bLocals = true;
    const bool bStatics = true;
    const bool bInScopeOnly = true;
    const lldb::SBValueList valueList =
        frame.GetVariables(bArgs, bLocals, bStatics, bInScopeOnly);
    value = valueList.GetFirstValueByName(rStrExpression.c_str());
  }

  if (!value.IsValid()) {
    value = frame.EvaluateExpression(rStrExpression.c_str());
    valueObjKind = ValObjKind_ec::eValObjKind_ConstResult;
  }

  if (value.IsValid() && value.GetError().Success()) {
    CompleteSBValue(value);
    m_bValid = true;
    m_nChildren = value.GetNumChildren();
    m_strType = CMICmnLLDBUtilSBValue(value).GetTypeNameDisplay();

    // This gets added to CMICmnLLDBDebugSessionInfoVarObj static container of
    // varObjs
    CMICmnLLDBDebugSessionInfoVarObj varObj(rStrExpression, m_strVarName, value,
                                            valueObjKind);
    m_strValue = varObj.GetValueFormatted();
  } else {
    m_strValue = value.GetError().GetCString();
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
bool CMICmdCmdVarCreate::Acknowledge() {
  if (m_bValid) {
    // MI print
    // "%s^done,name=\"%s\",numchild=\"%d\",value=\"%s\",type=\"%s\",thread-id=\"%llu\",has_more=\"%u\""
    const CMICmnMIValueConst miValueConst(m_strVarName);
    CMICmnMIValueResult miValueResultAll("name", miValueConst);
    const CMIUtilString strNumChild(CMIUtilString::Format("%d", m_nChildren));
    const CMICmnMIValueConst miValueConst2(strNumChild);
    miValueResultAll.Add("numchild", miValueConst2);
    const CMICmnMIValueConst miValueConst3(m_strValue);
    miValueResultAll.Add("value", miValueConst3);
    const CMICmnMIValueConst miValueConst4(m_strType);
    miValueResultAll.Add("type", miValueConst4);
    const CMIUtilString strThreadId(CMIUtilString::Format("%llu", m_nThreadId));
    const CMICmnMIValueConst miValueConst5(strThreadId);
    miValueResultAll.Add("thread-id", miValueConst5);
    const CMICmnMIValueConst miValueConst6("0");
    miValueResultAll.Add("has_more", miValueConst6);

    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
        miValueResultAll);
    m_miResultRecord = miRecordResult;

    return MIstatus::success;
  }

  CMIUtilString strErrMsg(m_strValue);
  if (m_strValue.empty())
    strErrMsg = CMIUtilString::Format(
        MIRSRC(IDS_CMD_ERR_VARIABLE_CREATION_FAILED), m_strExpression.c_str());
  const CMICmnMIValueConst miValueConst(
      strErrMsg.Escape(true /* vbEscapeQuotes */));
  CMICmnMIValueResult miValueResult("msg", miValueConst);
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
CMICmdBase *CMICmdCmdVarCreate::CreateSelf() {
  return new CMICmdCmdVarCreate();
}

//++
// Details: Complete SBValue object and its children to get
// SBValue::GetValueDidChange
//          work.
// Type:    Method.
// Args:    vrwValue    - (R)   Value to update.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
void CMICmdCmdVarCreate::CompleteSBValue(lldb::SBValue &vrwValue) {
  // Force a value to update
  vrwValue.GetValueDidChange();
  // Do not traverse the Children values. For reference, see
  // https://github.com/lldb-tools/lldb-mi/pull/115#discussion_r1706360623.
}

//++
// Details: CMICmdCmdVarUpdate constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarUpdate::CMICmdCmdVarUpdate()
    : m_constStrArgPrintValues("print-values"), m_constStrArgName("name"),
      m_bValueChanged(false), m_miValueList(true) {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-update";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarUpdate::CreateSelf;
}

//++
// Details: CMICmdCmdVarUpdate destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarUpdate::~CMICmdCmdVarUpdate() {}

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
bool CMICmdCmdVarUpdate::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValPrintValues(m_constStrArgPrintValues, false, true));
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true));
  return ParseValidateCmdOptions();
}

//++
// Details: Print an SBValue or its children into the changelist response. This
//          is a recursive function. Note that user code may contain infinite
//          recursion, if a structure contains a pointer to itself (directly or
//          indirectly) - but this code can't really break this recursion,
//          because user still may open up elements in variables view into many
//          levels of nesting. In practice recursion is stopped by the user who
//          at some point stops expanding nested elements, and this would mean
//          that at some point complex variable will not have its children
//          listed with -var-list-children and that will stop recursion.
// Return:  MIstatus::success - Function succeeded.
//          MIstatus::failure - Function failed.
//--
bool CMICmdCmdVarUpdate::PrintValue(
    CMICmnLLDBDebugSessionInfo::VariableInfoFormat_e eVarInfoFormat,
    lldb::SBValue &value, const CMIUtilString &valueName) {

  bool bPrintedChildren = false;
  if (value.MightHaveChildren()) {
    // Scan children recursively and print those that changed.
    const MIuint nChildren = value.GetNumChildren();
    for (MIuint i = 0; i < nChildren; i++) {
      lldb::SBValue child = value.GetChildAtIndex(i);

      // Did this child changed its value?
      bool childValueChanged = false;
      if (!ExamineSBValueForChange(child, childValueChanged))
        return MIstatus::failure;
      if (!childValueChanged)
        continue;

      // rValue.GetNumChildren() returns all members, but this function should
      // print only those that has been listed by -var-list-children or
      // -var-set-update-range. LLDB-MI doens't support the latter, though.
      // I don't know any better way to check if member has been listed than
      // to attempt to get the member by its name.
      const CMIUtilString childName(
          GetMemberName(valueName, CMICmnLLDBUtilSBValue(child).GetName(), i));
      CMICmnLLDBDebugSessionInfoVarObj childVarObj;
      if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(childName, childVarObj))
        continue;

      PrintValue(eVarInfoFormat, child, childName);
      bPrintedChildren = true;
    }
  }

  if (!bPrintedChildren ||
      (value.GetType().GetTypeFlags() & lldb::eTypeIsPointer &&
       value.GetValueDidChange())) {
    // Print scalar values or complex value if its children were not printed.
    // Pointer to a structure is also printed if its own value changed
    // (structure is printed as `{...}`, while pointer to a structure is printed
    // as an address value).
    const bool bPrintValue =
        ((eVarInfoFormat ==
          CMICmnLLDBDebugSessionInfo::eVariableInfoFormat_AllValues) ||
         (eVarInfoFormat ==
              CMICmnLLDBDebugSessionInfo::eVariableInfoFormat_SimpleValues &&
          value.GetNumChildren() == 0));
    const CMIUtilString strValue(
        CMICmnLLDBDebugSessionInfoVarObj::GetValueStringFormatted(
            value, CMICmnLLDBDebugSessionInfoVarObj::eVarFormat_Natural));
    const CMIUtilString strInScope(value.IsInScope() ? "true" : "false");

    MIFormResponse(valueName, bPrintValue ? strValue.c_str() : nullptr,
                   strInScope);
  }

  return MIstatus::success;
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
bool CMICmdCmdVarUpdate::Execute() {
  CMICMDBASE_GETOPTION(pArgPrintValues, PrintValues, m_constStrArgPrintValues);
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);

  CMICmnLLDBDebugSessionInfo::VariableInfoFormat_e eVarInfoFormat =
      CMICmnLLDBDebugSessionInfo::eVariableInfoFormat_NoValues;
  if (pArgPrintValues->GetFound())
    eVarInfoFormat =
        static_cast<CMICmnLLDBDebugSessionInfo::VariableInfoFormat_e>(
            pArgPrintValues->GetValue());

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  CMICmnLLDBDebugSessionInfoVarObj varObj;
  if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarObjName, varObj)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
    return MIstatus::failure;
  }

  lldb::SBValue &rValue = varObj.GetValue();
  if (!ExamineSBValueForChange(rValue, m_bValueChanged))
    return MIstatus::failure;

  if (!m_bValueChanged) {
    CMICmnLLDBDebugSessionInfo &rSessionInfo(
        CMICmnLLDBDebugSessionInfo::Instance());

    lldb::SBFrame frame =
        rSessionInfo.GetProcess().GetSelectedThread().GetSelectedFrame();
    if (!frame.IsValid()) {
      SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_FRAME_INVALID),
                                     m_constStrArgFrame.c_str()));
      return MIstatus::failure;
    }

    const auto valObjKind = varObj.GetValObjKind();
    if (valObjKind == ValObjKind_ec::eValObjKind_ConstResult) {
      // This is likely an expression result and it should be re-evaluated.

      auto tmpValue = frame.EvaluateExpression(varObj.GetNameReal().c_str());
      if (tmpValue.IsValid() && tmpValue.GetError().Success()) {
        m_bValueChanged = true;
        rValue = std::move(tmpValue);
      }
    }
  }

  if (m_bValueChanged) {
    varObj.UpdateValue();
    return PrintValue(eVarInfoFormat, rValue, rVarObjName);
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
bool CMICmdCmdVarUpdate::Acknowledge() {
  if (m_bValueChanged) {
    // MI print
    // "%s^done,changelist=[{name=\"%s\",value=\"%s\",in_scope=\"%s\",type_changed=\"false\",has_more=\"0\"}]"
    CMICmnMIValueResult miValueResult("changelist", m_miValueList);
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
        miValueResult);
    m_miResultRecord = miRecordResult;
  } else {
    // MI print "%s^done,changelist=[]"
    const CMICmnMIValueList miValueList(true);
    CMICmnMIValueResult miValueResult6("changelist", miValueList);
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
        miValueResult6);
    m_miResultRecord = miRecordResult;
  }

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
CMICmdBase *CMICmdCmdVarUpdate::CreateSelf() {
  return new CMICmdCmdVarUpdate();
}

//++
// Details: Form the MI response for multiple variables.
// Type:    Method.
// Args:    vrStrVarName    - (R)   Session var object's name.
//          vpValue         - (R)   Text version of the value held in the
//          variable.
//          vrStrScope      - (R)   In scope "yes" or "no".
// Return:  None.
// Throws:  None.
//--
void CMICmdCmdVarUpdate::MIFormResponse(const CMIUtilString &vrStrVarName,
                                        const char *const vpValue,
                                        const CMIUtilString &vrStrScope) {
  // MI print
  // "[{name=\"%s\",value=\"%s\",in_scope=\"%s\",type_changed=\"false\",has_more=\"0\"}]"
  const CMICmnMIValueConst miValueConst(vrStrVarName);
  const CMICmnMIValueResult miValueResult("name", miValueConst);
  CMICmnMIValueTuple miValueTuple(miValueResult);
  if (vpValue != nullptr) {
    const CMICmnMIValueConst miValueConst2(vpValue);
    const CMICmnMIValueResult miValueResult2("value", miValueConst2);
    miValueTuple.Add(miValueResult2);
  }
  const CMICmnMIValueConst miValueConst3(vrStrScope);
  const CMICmnMIValueResult miValueResult3("in_scope", miValueConst3);
  miValueTuple.Add(miValueResult3);
  const CMICmnMIValueConst miValueConst4("false");
  const CMICmnMIValueResult miValueResult4("type_changed", miValueConst4);
  miValueTuple.Add(miValueResult4);
  const CMICmnMIValueConst miValueConst5("0");
  const CMICmnMIValueResult miValueResult5("has_more", miValueConst5);
  miValueTuple.Add(miValueResult5);
  m_miValueList.Add(miValueTuple);
}

//++
// Details: Determine if the var object was changed.
// Type:    Method.
// Args:    vrVarObj    - (R)   Session var object to examine.
//          vrwbChanged - (W)   True = The var object was changed,
//                              False = It was not changed.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdCmdVarUpdate::ExamineSBValueForChange(lldb::SBValue &vrwValue,
                                                 bool &vrwbChanged) {
  vrwbChanged = vrwValue.GetValueDidChange();
  // Do not traverse the Children values. For reference, see
  // https://github.com/lldb-tools/lldb-mi/pull/115#discussion_r1706367538.

  return MIstatus::success;
}

//++
// Details: CMICmdCmdVarDelete constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarDelete::CMICmdCmdVarDelete() : m_constStrArgName("name") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-delete";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarDelete::CreateSelf;
}

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
bool CMICmdCmdVarDelete::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true));
  return ParseValidateCmdOptions();
}

//++
// Details: CMICmdCmdVarDelete destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarDelete::~CMICmdCmdVarDelete() {}

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
bool CMICmdCmdVarDelete::Execute() {
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  CMICmnLLDBDebugSessionInfoVarObj::VarObjDelete(rVarObjName);

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
bool CMICmdCmdVarDelete::Acknowledge() {
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
CMICmdBase *CMICmdCmdVarDelete::CreateSelf() {
  return new CMICmdCmdVarDelete();
}

//++
// Details: CMICmdCmdVarAssign constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarAssign::CMICmdCmdVarAssign()
    : m_bOk(true), m_constStrArgName("name"),
      m_constStrArgExpression("expression") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-assign";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarAssign::CreateSelf;
}

//++
// Details: CMICmdCmdVarAssign destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarAssign::~CMICmdCmdVarAssign() {}

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
bool CMICmdCmdVarAssign::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true));
  m_setCmdArgs.Add(new CMICmdArgValText(m_constStrArgExpression, true, true));
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
bool CMICmdCmdVarAssign::Execute() {
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);
  CMICMDBASE_GETOPTION(pArgExpression, String, m_constStrArgExpression);

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  const CMIUtilString &rExpression(pArgExpression->GetValue());

  CMICmnLLDBDebugSessionInfoVarObj varObj;
  if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarObjName, varObj)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
    return MIstatus::failure;
  }
  m_varObjName = rVarObjName;

  CMIUtilString strExpression(rExpression.Trim());
  strExpression = strExpression.Trim('"');
  lldb::SBValue &rValue(varObj.GetValue());
  m_bOk = rValue.SetValueFromCString(strExpression.c_str());
  if (m_bOk)
    varObj.UpdateValue();

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
bool CMICmdCmdVarAssign::Acknowledge() {
  if (m_bOk) {
    // MI print "%s^done,value=\"%s\""
    CMICmnLLDBDebugSessionInfoVarObj varObj;
    CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(m_varObjName, varObj);
    const CMICmnMIValueConst miValueConst(varObj.GetValueFormatted());
    const CMICmnMIValueResult miValueResult("value", miValueConst);
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
        miValueResult);
    m_miResultRecord = miRecordResult;

    return MIstatus::success;
  }

  const CMICmnMIValueConst miValueConst("expression could not be evaluated");
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
CMICmdBase *CMICmdCmdVarAssign::CreateSelf() {
  return new CMICmdCmdVarAssign();
}

//++
// Details: CMICmdCmdVarSetFormat constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarSetFormat::CMICmdCmdVarSetFormat()
    : m_constStrArgName("name"), m_constStrArgFormatSpec("format-spec") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-set-format";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarSetFormat::CreateSelf;
}

//++
// Details: CMICmdCmdVarSetFormat destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarSetFormat::~CMICmdCmdVarSetFormat() {}

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
bool CMICmdCmdVarSetFormat::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true));
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgFormatSpec, true, true));
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
bool CMICmdCmdVarSetFormat::Execute() {
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);
  CMICMDBASE_GETOPTION(pArgFormatSpec, String, m_constStrArgFormatSpec);

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  const CMIUtilString &rExpression(pArgFormatSpec->GetValue());

  CMICmnLLDBDebugSessionInfoVarObj varObj;
  if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarObjName, varObj)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
    return MIstatus::failure;
  }
  if (!varObj.SetVarFormat(
          CMICmnLLDBDebugSessionInfoVarObj::GetVarFormatForString(
              rExpression))) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_ENUM_INVALID),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str(), rExpression.c_str()));
    return MIstatus::failure;
  }
  varObj.UpdateValue();

  m_varObjName = rVarObjName;

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
bool CMICmdCmdVarSetFormat::Acknowledge() {
  // MI print
  // "%s^done,changelist=[{name=\"%s\",value=\"%s\",in_scope=\"%s\",type_changed=\"false\",has_more=\"0\"}]"
  CMICmnLLDBDebugSessionInfoVarObj varObj;
  CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(m_varObjName, varObj);
  const CMICmnMIValueConst miValueConst(m_varObjName);
  const CMICmnMIValueResult miValueResult("name", miValueConst);
  CMICmnMIValueTuple miValueTuple(miValueResult);
  const CMICmnMIValueConst miValueConst2(varObj.GetValueFormatted());
  const CMICmnMIValueResult miValueResult2("value", miValueConst2);
  miValueTuple.Add(miValueResult2);
  lldb::SBValue &rValue = const_cast<lldb::SBValue &>(varObj.GetValue());
  const CMICmnMIValueConst miValueConst3(rValue.IsInScope() ? "true" : "false");
  const CMICmnMIValueResult miValueResult3("in_scope", miValueConst3);
  miValueTuple.Add(miValueResult3);
  const CMICmnMIValueConst miValueConst4("false");
  const CMICmnMIValueResult miValueResult4("type_changed", miValueConst4);
  miValueTuple.Add(miValueResult4);
  const CMICmnMIValueConst miValueConst5("0");
  const CMICmnMIValueResult miValueResult5("type_changed", miValueConst5);
  miValueTuple.Add(miValueResult5);
  const CMICmnMIValueList miValueList(miValueTuple);
  const CMICmnMIValueResult miValueResult6("changelist", miValueList);

  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
      miValueResult6);
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
CMICmdBase *CMICmdCmdVarSetFormat::CreateSelf() {
  return new CMICmdCmdVarSetFormat();
}

//++
// Details: CMICmdCmdVarListChildren constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarListChildren::CMICmdCmdVarListChildren()
    : m_constStrArgPrintValues("print-values"), m_constStrArgName("name"),
      m_constStrArgFrom("from"), m_constStrArgTo("to"), m_bValueValid(false),
      m_nChildren(0), m_miValueList(true), m_bHasMore(false) {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-list-children";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarListChildren::CreateSelf;
}

//++
// Details: CMICmdCmdVarListChildren destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarListChildren::~CMICmdCmdVarListChildren() {}

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
bool CMICmdCmdVarListChildren::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValPrintValues(m_constStrArgPrintValues, false, true));
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true, true));
  m_setCmdArgs.Add(new CMICmdArgValNumber(m_constStrArgFrom, false, true));
  m_setCmdArgs.Add(new CMICmdArgValNumber(m_constStrArgTo, false, true));
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
bool CMICmdCmdVarListChildren::Execute() {
  CMICMDBASE_GETOPTION(pArgPrintValues, PrintValues, m_constStrArgPrintValues);
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);
  CMICMDBASE_GETOPTION(pArgFrom, Number, m_constStrArgFrom);
  CMICMDBASE_GETOPTION(pArgTo, Number, m_constStrArgTo);

  CMICmnLLDBDebugSessionInfo::VariableInfoFormat_e eVarInfoFormat =
      CMICmnLLDBDebugSessionInfo::eVariableInfoFormat_NoValues;
  if (pArgPrintValues->GetFound())
    eVarInfoFormat =
        static_cast<CMICmnLLDBDebugSessionInfo::VariableInfoFormat_e>(
            pArgPrintValues->GetValue());

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  CMICmnLLDBDebugSessionInfoVarObj varObj;
  if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarObjName, varObj)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
    return MIstatus::failure;
  }

  MIuint nFrom = 0;
  MIuint nTo = UINT32_MAX;
  if (pArgFrom->GetFound() && pArgTo->GetFound()) {
    nFrom = pArgFrom->GetValue();
    nTo = pArgTo->GetValue();
  } else if (pArgFrom->GetFound() || pArgTo->GetFound()) {
    // Only from or to was specified but both are required
    SetError(
        CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_CHILD_RANGE_INVALID),
                              m_cmdData.strMiCmd.c_str()));
    return MIstatus::failure;
  }

  lldb::SBValue &rValue = const_cast<lldb::SBValue &>(varObj.GetValue());
  m_bValueValid = rValue.IsValid();
  if (!m_bValueValid)
    return MIstatus::success;

  const MIuint nChildren = rValue.GetNumChildren();
  m_bHasMore = nTo < nChildren;
  nTo = std::min(nTo, nChildren);
  m_nChildren = nFrom < nTo ? nTo - nFrom : 0;
  for (MIuint i = nFrom; i < nTo; i++) {
    lldb::SBValue member = rValue.GetChildAtIndex(i);
    const CMICmnLLDBUtilSBValue utilValue(member);
    const CMIUtilString strExp = utilValue.GetName();
    const CMIUtilString name(GetMemberName(rVarObjName, strExp, i));
    const MIuint nChildren = member.GetNumChildren();
    const CMIUtilString strThreadId(
        CMIUtilString::Format("%u", member.GetThread().GetIndexID()));

    // Varobj gets added to CMICmnLLDBDebugSessionInfoVarObj static container of
    // varObjs
    CMICmnLLDBDebugSessionInfoVarObj var(strExp, name, member, rVarObjName,
                                         varObj.GetValObjKind());

    // MI print
    // "child={name=\"%s\",exp=\"%s\",numchild=\"%d\",value=\"%s\",type=\"%s\",thread-id=\"%u\",has_more=\"%u\"}"
    const CMICmnMIValueConst miValueConst(name);
    const CMICmnMIValueResult miValueResult("name", miValueConst);
    CMICmnMIValueTuple miValueTuple(miValueResult);
    const CMICmnMIValueConst miValueConst2(strExp);
    const CMICmnMIValueResult miValueResult2("exp", miValueConst2);
    miValueTuple.Add(miValueResult2);
    const CMIUtilString strNumChild(CMIUtilString::Format("%u", nChildren));
    const CMICmnMIValueConst miValueConst3(strNumChild);
    const CMICmnMIValueResult miValueResult3("numchild", miValueConst3);
    miValueTuple.Add(miValueResult3);
    const CMICmnMIValueConst miValueConst5(utilValue.GetTypeNameDisplay());
    const CMICmnMIValueResult miValueResult5("type", miValueConst5);
    miValueTuple.Add(miValueResult5);
    const CMICmnMIValueConst miValueConst6(strThreadId);
    const CMICmnMIValueResult miValueResult6("thread-id", miValueConst6);
    miValueTuple.Add(miValueResult6);
    // nChildren == 0 is used to check for simple values
    if (eVarInfoFormat ==
            CMICmnLLDBDebugSessionInfo::eVariableInfoFormat_AllValues ||
        (eVarInfoFormat ==
             CMICmnLLDBDebugSessionInfo::eVariableInfoFormat_SimpleValues &&
         nChildren == 0)) {
      const CMIUtilString strValue(
          CMICmnLLDBDebugSessionInfoVarObj::GetValueStringFormatted(
              member, CMICmnLLDBDebugSessionInfoVarObj::eVarFormat_Natural));
      const CMICmnMIValueConst miValueConst7(strValue);
      const CMICmnMIValueResult miValueResult7("value", miValueConst7);
      miValueTuple.Add(miValueResult7);
    }
    const CMICmnMIValueConst miValueConst8("0");
    const CMICmnMIValueResult miValueResult8("has_more", miValueConst8);
    miValueTuple.Add(miValueResult8);
    const CMICmnMIValueResult miValueResult9("child", miValueTuple);
    m_miValueList.Add(miValueResult9);
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
bool CMICmdCmdVarListChildren::Acknowledge() {
  if (m_bValueValid) {
    // MI print "%s^done,numchild=\"%u\",children=[%s],has_more=\"%d\""
    const CMIUtilString strNumChild(CMIUtilString::Format("%u", m_nChildren));
    const CMICmnMIValueConst miValueConst(strNumChild);
    CMICmnMIValueResult miValueResult("numchild", miValueConst);
    if (m_nChildren != 0)
      miValueResult.Add("children", m_miValueList);
    const CMIUtilString strHasMore(m_bHasMore ? "1" : "0");
    const CMICmnMIValueConst miValueConst2(strHasMore);
    miValueResult.Add("has_more", miValueConst2);

    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
        miValueResult);
    m_miResultRecord = miRecordResult;

    return MIstatus::success;
  }

  // MI print "%s^error,msg=\"variable invalid\""
  const CMICmnMIValueConst miValueConst("variable invalid");
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
CMICmdBase *CMICmdCmdVarListChildren::CreateSelf() {
  return new CMICmdCmdVarListChildren();
}

//++
// Details: CMICmdCmdVarEvaluateExpression constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarEvaluateExpression::CMICmdCmdVarEvaluateExpression()
    : m_bValueValid(true), m_constStrArgFormatSpec("-f"),
      m_constStrArgName("name") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-evaluate-expression";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarEvaluateExpression::CreateSelf;
}

//++
// Details: CMICmdCmdVarEvaluateExpression destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarEvaluateExpression::~CMICmdCmdVarEvaluateExpression() {}

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
bool CMICmdCmdVarEvaluateExpression::ParseArgs() {
  m_setCmdArgs.Add(
      new CMICmdArgValOptionShort(m_constStrArgFormatSpec, false, false,
                                  CMICmdArgValListBase::eArgValType_String, 1));
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true));
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
bool CMICmdCmdVarEvaluateExpression::Execute() {
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  CMICmnLLDBDebugSessionInfoVarObj varObj;
  if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarObjName, varObj)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
    return MIstatus::failure;
  }

  lldb::SBValue &rValue = const_cast<lldb::SBValue &>(varObj.GetValue());
  m_bValueValid = rValue.IsValid();
  if (!m_bValueValid)
    return MIstatus::success;

  m_varObjName = rVarObjName;
  varObj.UpdateValue();

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
bool CMICmdCmdVarEvaluateExpression::Acknowledge() {
  if (m_bValueValid) {
    CMICmnLLDBDebugSessionInfoVarObj varObj;
    CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(m_varObjName, varObj);
    const CMICmnMIValueConst miValueConst(varObj.GetValueFormatted());
    const CMICmnMIValueResult miValueResult("value", miValueConst);
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
        miValueResult);
    m_miResultRecord = miRecordResult;
    return MIstatus::success;
  }

  const CMICmnMIValueConst miValueConst("variable invalid");
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
CMICmdBase *CMICmdCmdVarEvaluateExpression::CreateSelf() {
  return new CMICmdCmdVarEvaluateExpression();
}

//++
// Details: CMICmdCmdVarInfoPathExpression constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarInfoPathExpression::CMICmdCmdVarInfoPathExpression()
    : m_bValueValid(true), m_constStrArgName("name") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-info-path-expression";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarInfoPathExpression::CreateSelf;
}

//++
// Details: CMICmdCmdVarInfoPathExpression destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarInfoPathExpression::~CMICmdCmdVarInfoPathExpression() {}

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
bool CMICmdCmdVarInfoPathExpression::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true));
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
bool CMICmdCmdVarInfoPathExpression::Execute() {
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  CMICmnLLDBDebugSessionInfoVarObj varObj;
  if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarObjName, varObj)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
    return MIstatus::failure;
  }

  lldb::SBValue &rValue = const_cast<lldb::SBValue &>(varObj.GetValue());
  m_bValueValid = rValue.IsValid();
  if (!m_bValueValid)
    return MIstatus::success;

  lldb::SBStream stream;
  if (!rValue.GetExpressionPath(stream, true)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_EXPRESSIONPATH),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
    return MIstatus::failure;
  }

  const char *pPathExpression = stream.GetData();
  if (pPathExpression == nullptr) {
    // Build expression from what we do know
    m_strPathExpression = varObj.GetNameReal();
    return MIstatus::success;
  }

  // Has LLDB returned a var signature of it's own
  if (pPathExpression[0] != '$') {
    m_strPathExpression = pPathExpression;
    return MIstatus::success;
  }

  // Build expression from what we do know
  const CMIUtilString &rVarParentName(varObj.GetVarParentName());
  if (rVarParentName.empty()) {
    m_strPathExpression = varObj.GetNameReal();
  } else {
    CMICmnLLDBDebugSessionInfoVarObj varObjParent;
    if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarParentName,
                                                     varObjParent)) {
      SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                     m_cmdData.strMiCmd.c_str(),
                                     rVarParentName.c_str()));
      return MIstatus::failure;
    }
    m_strPathExpression =
        CMIUtilString::Format("%s.%s", varObjParent.GetNameReal().c_str(),
                              varObj.GetNameReal().c_str());
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
bool CMICmdCmdVarInfoPathExpression::Acknowledge() {
  if (m_bValueValid) {
    const CMICmnMIValueConst miValueConst(m_strPathExpression);
    const CMICmnMIValueResult miValueResult("path_expr", miValueConst);
    const CMICmnMIResultRecord miRecordResult(
        m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
        miValueResult);
    m_miResultRecord = miRecordResult;
    return MIstatus::success;
  }

  const CMICmnMIValueConst miValueConst("variable invalid");
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
CMICmdBase *CMICmdCmdVarInfoPathExpression::CreateSelf() {
  return new CMICmdCmdVarInfoPathExpression();
}

//++
// Details: CMICmdCmdVarShowAttributes constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarShowAttributes::CMICmdCmdVarShowAttributes()
    : m_constStrArgName("name") {
  // Command factory matches this name with that received from the stdin stream
  m_strMiCmd = "var-show-attributes";

  // Required by the CMICmdFactory when registering *this command
  m_pSelfCreatorFn = &CMICmdCmdVarShowAttributes::CreateSelf;
}

//++
// Details: CMICmdCmdVarShowAttributes destructor.
// Type:    Overrideable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmdCmdVarShowAttributes::~CMICmdCmdVarShowAttributes() {}

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
bool CMICmdCmdVarShowAttributes::ParseArgs() {
  m_setCmdArgs.Add(new CMICmdArgValString(m_constStrArgName, true, true));
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
bool CMICmdCmdVarShowAttributes::Execute() {
  CMICMDBASE_GETOPTION(pArgName, String, m_constStrArgName);

  const CMIUtilString &rVarObjName(pArgName->GetValue());
  CMICmnLLDBDebugSessionInfoVarObj varObj;
  if (!CMICmnLLDBDebugSessionInfoVarObj::VarObjGet(rVarObjName, varObj)) {
    SetError(CMIUtilString::Format(MIRSRC(IDS_CMD_ERR_VARIABLE_DOESNOTEXIST),
                                   m_cmdData.strMiCmd.c_str(),
                                   rVarObjName.c_str()));
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
bool CMICmdCmdVarShowAttributes::Acknowledge() {
  // MI output: "%s^done,status=\"editable\"]"
  const CMICmnMIValueConst miValueConst("editable");
  const CMICmnMIValueResult miValueResult("status", miValueConst);
  const CMICmnMIResultRecord miRecordResult(
      m_cmdData.strMiCmdToken, CMICmnMIResultRecord::eResultClass_Done,
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
CMICmdBase *CMICmdCmdVarShowAttributes::CreateSelf() {
  return new CMICmdCmdVarShowAttributes();
}
