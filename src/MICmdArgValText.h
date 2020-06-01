//===-- MICmdArgValText.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

// In-house headers:
#include "MICmdArgValBase.h"

// Declarations:
class CMICmdArgContext;

//++
//============================================================================
// Details: MI common code class. Command argument class. Arguments object
//          needing specialization derived from the CMICmdArgValBase class.
//          An argument knows what type of argument it is and how it is to
//          interpret the options (context) string to find and validate a
//          matching argument and so extract a value from it.
//
//          Extracts an argument as is removing only surrounding quotes.
//
//          Based on the Interpreter pattern.
//--
class CMICmdArgValText : public CMICmdArgValBaseTemplate<CMIUtilString> {
  // Methods:
public:
  using CMICmdArgValBaseTemplate::CMICmdArgValBaseTemplate;
  // Overridden:
public:
  // From CMICmdArgValBase
  /* dtor */ ~CMICmdArgValText() override = default;
  // From CMICmdArgSet::IArg
  bool Validate(CMICmdArgContext &vwrArgContext) override;

  // Utilities:
protected:
  bool ConsumeArgument(CMICmdArgContext &vwrArgContext,
                       const CMIUtilString &vrArg);
};
