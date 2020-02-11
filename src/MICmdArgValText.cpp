//===-- MICmdArgValText.cpp -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// In-house headers:
#include "MICmdArgValText.h"
#include "MICmdArgContext.h"

//++
// Details: Parse the command's argument options string and try to extract the
//          value *this argument is looking for.
// Type:    Overridden.
// Args:    vwArgContext    - (R) The command's argument options string.
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdArgValText::Validate(CMICmdArgContext &vwrArgContext) {
  if (vwrArgContext.IsEmpty())
    return m_bMandatory ? MIstatus::failure : MIstatus::success;

  auto &&args = vwrArgContext.GetArgs();

  auto it = args.begin();
  if (it == args.end())
    return MIstatus::failure;

  const CMIUtilString &rExpr(*it);

  return ConsumeArgument(vwrArgContext, rExpr);
}

//++
// Details: Remove the given argument from the context and store it as the
//          value of this argument.
// Type:    Overridden.
// Args:    vwArgContext    - (R) The command's argument options string.
//          vrArg           - (R) The argument to consume
// Return:  MIstatus::success - Functional succeeded.
//          MIstatus::failure - Functional failed.
// Throws:  None.
//--
bool CMICmdArgValText::ConsumeArgument(CMICmdArgContext &vwrArgContext,
                                       const CMIUtilString &vrArg) {
  // We must do it before removing because reference can become invalid
  auto &&rArgValue = vrArg.Trim().Trim('"').StripSlashes();

  if (vwrArgContext.RemoveArg(vrArg)) {
    m_bValid = true;
    m_argValue = rArgValue;
    return MIstatus::success;
  }

  return MIstatus::failure;
}
