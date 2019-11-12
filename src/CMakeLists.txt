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

find_library(liblldb lldb HINTS ${LLVM_LIBRARY_DIRS})
find_library(libllvm LLVM HINTS ${LLVM_LIBRARY_DIRS})
if (NOT libllvm)
  message(STATUS "Can't find LLVM shared library, falling back to static linking LLVMSupport")
  find_library(libllvm LLVMSupport HINTS ${LLVM_LIBRARY_DIRS})
  find_package(Curses REQUIRED)
endif()

target_link_libraries(lldb-mi ${liblldb} ${libllvm} ${LLVM_PTHREAD_LIB} ${CURSES_LIBRARIES})
