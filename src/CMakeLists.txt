add_executable(lldb-mi
  MICmdArgContext.cpp
  MICmdArgSet.cpp
  MICmdArgValBase.cpp
  MICmdArgValConsume.cpp
  MICmdArgValFile.cpp
  MICmdArgValListBase.cpp
  MICmdArgValListOfN.cpp
  MICmdArgValNumber.cpp
  MICmdArgValOptionLong.cpp
  MICmdArgValOptionShort.cpp
  MICmdArgValPrintValues.cpp
  MICmdArgValString.cpp
  MICmdArgValText.cpp
  MICmdArgValThreadGrp.cpp
  MICmdBase.cpp
  MICmdCommands.cpp
  MICmdCmd.cpp
  MICmdCmdBreak.cpp
  MICmdCmdData.cpp
  MICmdCmdEnviro.cpp
  MICmdCmdExec.cpp
  MICmdCmdFile.cpp
  MICmdCmdGdbInfo.cpp
  MICmdCmdGdbSet.cpp
  MICmdCmdGdbShow.cpp
  MICmdCmdGdbThread.cpp
  MICmdCmdMiscellanous.cpp
  MICmdCmdStack.cpp
  MICmdCmdSupportInfo.cpp
  MICmdCmdSupportList.cpp
  MICmdCmdSymbol.cpp
  MICmdCmdTarget.cpp
  MICmdCmdThread.cpp
  MICmdCmdTrace.cpp
  MICmdCmdVar.cpp
  MICmdData.cpp
  MICmdFactory.cpp
  MICmdInterpreter.cpp
  MICmdInvoker.cpp
  MICmdMgr.cpp
  MICmdMgrSetCmdDeleteCallback.cpp
  MICmnBase.cpp
  MICmnLLDBBroadcaster.cpp
  MICmnLLDBDebugger.cpp
  MICmnLLDBDebuggerHandleEvents.cpp
  MICmnLLDBDebugSessionInfo.cpp
  MICmnLLDBDebugSessionInfoVarObj.cpp
  MICmnLLDBProxySBValue.cpp
  MICmnLLDBUtilSBValue.cpp
  MICmnLog.cpp
  MICmnLogMediumFile.cpp
  MICmnMIOutOfBandRecord.cpp
  MICmnMIResultRecord.cpp
  MICmnMIValue.cpp
  MICmnMIValueConst.cpp
  MICmnMIValueList.cpp
  MICmnMIValueResult.cpp
  MICmnMIValueTuple.cpp
  MICmnResources.cpp
  MICmnStreamStderr.cpp
  MICmnStreamStdin.cpp
  MICmnStreamStdout.cpp
  MICmnThreadMgrStd.cpp
  MIDriver.cpp
  MIDriverBase.cpp
  MIDriverMain.cpp
  MIDriverMgr.cpp
  MIUtilDateTimeStd.cpp
  MIUtilDebug.cpp
  MIUtilFileStd.cpp
  MIUtilMapIdToVariant.cpp
  MIUtilString.cpp
  MIUtilThreadBaseStd.cpp
  MIUtilVariant.cpp
)

set(llvm_deps "")

set_property(TARGET lldb-mi PROPERTY CXX_STANDARD 14)

if (USE_LLDB_FRAMEWORK)
  find_library(lib_lldb NAMES LLDB PATHS ${LLVM_BINARY_DIR}/Library/Frameworks)
else()
  find_library(lib_lldb NAMES lldb liblldb HINTS ${LLVM_LIBRARY_DIRS})
endif()

find_library(lib_llvm LLVM HINTS ${LLVM_LIBRARY_DIRS})

if (NOT lib_llvm)
  message(STATUS "Can't find LLVM shared library, falling back to static linking LLVMSupport")
  find_library(lib_llvm LLVMSupport HINTS ${LLVM_LIBRARY_DIRS})

  get_property(LLVMSupportDeps GLOBAL PROPERTY LLVMBUILD_LIB_DEPS_LLVMSupport)
  foreach(dependency ${LLVMSupportDeps})
    find_library(lib ${dependency} HINTS ${LLVM_LIBRARY_DIRS})
    list(APPEND llvm_deps "${lib}")
  endforeach()
  if(NOT MSVC)
    find_package(Curses REQUIRED)
    list(APPEND llvm_deps "${CURSES_LIBRARIES}")
  endif()
endif()

list(APPEND llvm_deps "${LLVM_PTHREAD_LIB}")

target_link_libraries(lldb-mi ${lib_lldb} ${lib_llvm} ${llvm_deps})
install(TARGETS lldb-mi
  RUNTIME DESTINATION bin)
