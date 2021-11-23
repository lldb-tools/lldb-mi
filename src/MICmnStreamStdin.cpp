//===-- MICmnStreamStdin.cpp ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Third Party Headers
#ifdef _WIN32
#include "Platform.h"
#endif
#include <iostream>
#include <string.h>

// In-house headers:
#include "MICmnLog.h"
#include "MICmnResources.h"
#include "MICmnStreamStdin.h"
#include "MICmnStreamStdout.h"
#include "MIDriver.h"
#include "MIUtilSingletonHelper.h"

//++
// Details: CMICmnStreamStdin constructor.
// Type:    Method.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmnStreamStdin::CMICmnStreamStdin()
    : m_strPromptCurrent("(gdb)"), m_bShowPrompt(true) {}

//++
// Details: CMICmnStreamStdin destructor.
// Type:    Overridable.
// Args:    None.
// Return:  None.
// Throws:  None.
//--
CMICmnStreamStdin::~CMICmnStreamStdin() { Shutdown(); }

//++
// Details: Initialize resources for *this Stdin stream.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmnStreamStdin::Initialize() {
  m_clientUsageRefCnt++;

  if (m_bInitialized)
    return MIstatus::success;

  bool bOk = MIstatus::success;
  CMIUtilString errMsg;

  // Note initialisation order is important here as some resources depend on
  // previous
  MI::ModuleInit<CMICmnLog>(IDS_MI_INIT_ERR_LOG, bOk, errMsg);
  MI::ModuleInit<CMICmnResources>(IDS_MI_INIT_ERR_RESOURCES, bOk, errMsg);

  if (!bOk) {
    CMIUtilString strInitError(CMIUtilString::Format(
        MIRSRC(IDS_MI_INIT_ERR_STREAMSTDIN), errMsg.c_str()));
    SetErrorDescription(strInitError);

    return MIstatus::failure;
  }
  m_bInitialized = bOk;

  return MIstatus::success;
}

//++
// Details: Release resources for *this Stdin stream.
// Type:    Method.
// Args:    None.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmnStreamStdin::Shutdown() {
  if (--m_clientUsageRefCnt > 0)
    return MIstatus::success;

  if (!m_bInitialized)
    return MIstatus::success;

  m_bInitialized = false;

  ClrErrorDescription();

  bool bOk = MIstatus::success;
  CMIUtilString errMsg;

  MI::ModuleShutdown<CMICmnResources>(IDE_MI_SHTDWN_ERR_RESOURCES, bOk, errMsg);
  MI::ModuleShutdown<CMICmnLog>(IDS_MI_SHTDWN_ERR_LOG, bOk, errMsg);

  if (!bOk) {
    SetErrorDescriptionn(MIRSRC(IDE_MI_SHTDWN_ERR_STREAMSTDIN), errMsg.c_str());
  }

  return MIstatus::success;
}

//++
// Details: Validate and set the text that forms the prompt on the command line.
// Type:    Method.
// Args:    vNewPrompt  - (R) Text description.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmnStreamStdin::SetPrompt(const CMIUtilString &vNewPrompt) {
  if (vNewPrompt.empty()) {
    const CMIUtilString msg(CMIUtilString::Format(
        MIRSRC(IDS_STDIN_ERR_INVALID_PROMPT), vNewPrompt.c_str()));
    CMICmnStreamStdout::Instance().Write(msg);
    return MIstatus::failure;
  }

  m_strPromptCurrent = vNewPrompt;

  return MIstatus::success;
}

//++
// Details: Retrieve the command line prompt text currently being used.
// Type:    Method.
// Args:    None.
// Return:  const CMIUtilString & - Functional failed.
// Throws:  None.
//--
const CMIUtilString &CMICmnStreamStdin::GetPrompt() const {
  return m_strPromptCurrent;
}

//++
// Details: Set whether to display optional command line prompt. The prompt is
// output to
//          stdout. Disable it when this may interfere with the client reading
//          stdout as
//          input and it tries to interpret the prompt text to.
// Type:    Method.
// Args:    vbYes   - (R) True = Yes prompt is shown/output to the user
// (stdout), false = no prompt.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
void CMICmnStreamStdin::SetEnablePrompt(const bool vbYes) {
  m_bShowPrompt = vbYes;
}

//++
// Details: Get whether to display optional command line prompt. The prompt is
// output to
//          stdout. Disable it when this may interfere with the client reading
//          stdout as
//          input and it tries to interpret the prompt text to.
// Type:    Method.
// Args:    None.
// Return:  bool - True = Yes prompt is shown/output to the user (stdout), false
// = no prompt.
// Throws:  None.
//--
bool CMICmnStreamStdin::GetEnablePrompt() const { return m_bShowPrompt; }

//++
// Details: Wait on new line of data from stdin stream (completed by '\n' or
// '\r').
// Type:    Method.
// Args:    vwErrMsg    - (W) Empty string ok or error description.
// Return:  char * - text buffer pointer or NULL on failure.
// Throws:  None.
//--
const char *CMICmnStreamStdin::ReadLine(CMIUtilString &vwErrMsg) {
  vwErrMsg.clear();

  std::getline(std::cin, m_pCmdString);

  if (std::cin.eof()) {
#ifdef _MSC_VER
    // Was Ctrl-C hit?
    // On Windows, Ctrl-C gives an ERROR_OPERATION_ABORTED as error on the
    // command-line and the end-of-file indicator is also set.
    if (::GetLastError() == ERROR_OPERATION_ABORTED)
      return nullptr;
#endif
    const bool bForceExit = true;
    CMIDriver::Instance().SetExitApplicationFlag(bForceExit);
  } else if (std::cin.fail()) {
    vwErrMsg = ::strerror(errno);
    return nullptr;
  }

  return m_pCmdString.c_str();
}
