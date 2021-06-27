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

find_library(lib_llvm_shared LLVM HINTS ${LLVM_LIBRARY_DIRS})

if (lib_llvm_shared)
  set(lib_llvm "${lib_llvm_shared}")
else()
  message(STATUS "Can't find LLVM shared library, falling back to static linking LLVMSupport")
  llvm_map_components_to_libnames(lib_llvm support)

  if (LLVM_ENABLE_TERMINFO AND NOT WIN32)
    find_library(TERMINFO_LIB NAMES terminfo tinfo curses ncurses ncursesw)
    if (TERMINFO_LIB)
      list(APPEND llvm_deps "${TERMINFO_LIB}")
    endif()
  endif()
endif()

list(APPEND llvm_deps "${LLVM_PTHREAD_LIB}")

target_link_libraries(lldb-mi ${lib_lldb} ${lib_llvm} ${llvm_deps})
install(TARGETS lldb-mi
  RUNTIME DESTINATION bin)
