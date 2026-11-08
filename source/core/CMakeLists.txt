set (star_core_SOURCES
    StarDataStreamDevices.cpp
    StarJsonExtra.cpp
    StarLuaConverters.cpp
    scripting/StarUtilityLuaBindings.cpp
  )


configure_file (StarVersion.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/StarVersion.cpp)
add_library (star_core OBJECT ${star_core_SOURCES})
target_include_directories (star_core PRIVATE ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES})

# All importers share the bundled standard-library module from the parent.
# Thin global-type interfaces retain ordinary companions below;
# naturally self-contained core interfaces remain single-file.
add_library (star_modules STATIC)
target_sources (star_modules PUBLIC FILE_SET CXX_MODULES
    FILES StarVlqEncoding.cppm StarCurve25519.cppm
          StarBTree.cppm StarCasting.cppm StarStrongTypedef.cppm StarTypeIdentity.cppm
          StarDataStreamDevices.cppm StarDataStreamExtra.cppm
          StarJsonExtra.cppm StarLuaConverters.cppm StarNetImpl.cppm
          scripting/StarUtilityLuaBindings.cppm
          StarFormat.cppm StarTextFormatter.cppm
          StarHash.cppm StarFlatHashTable.cppm StarFlatHashMap.cppm StarFlatHashSet.cppm
          StarOptional.cppm StarList.cppm StarMap.cppm StarSet.cppm
          StarOrderedMap.cppm StarOrderedSet.cppm
          StarString.cppm StarStringView.cppm StarThread.cppm StarBiMap.cppm
          StarUniqueFunction.cppm
          StarIODevice.cppm StarFile.cppm StarDataStream.cppm StarXXHash.cppm
          StarJson.cppm StarLogging.cppm StarLua.cppm
          StarSha256.cppm StarImageScaling.cppm StarSecureRandom.cppm
          StarSignalHandler.cppm StarDynamicLib.cppm StarHttpClient.cppm StarUdp.cppm
          StarNativeRuntime.cppm
          StarIODeviceCallbacks.cppm StarCompression.cppm StarJsonPatch.cppm
          StarTickRateMonitor.cppm
          StarUuid.cppm StarHostAddress.cppm StarSocket.cppm StarTcp.cppm
          StarWorkerPool.cppm StarOptionParser.cppm StarShellParser.cppm
          StarJsonPath.cppm
          StarJsonParser.cppm StarJsonBuilder.cppm StarFormattedJson.cppm
          StarRpcPromise.cppm StarJsonRpc.cppm
          StarNetElement.cppm StarNetElementBasicFields.cppm
          StarNetElementContainers.cppm StarNetElementDynamicGroup.cppm
          StarNetElementExt.cppm StarNetElementFloatFields.cppm
          StarNetElementGroup.cppm StarNetElementSignal.cppm
          StarNetElementSyncGroup.cppm StarNetElementSystem.cppm StarNetElementTop.cppm
          StarAssetPath.cppm
          StarBTreeDatabase.cppm
          StarBuffer.cppm
          StarDirectives.cppm
          StarEncode.cppm
          StarImageProcessing.cppm
          StarLockFile.cppm
          StarPerlin.cppm
          StarText.cppm
          StarTime.cppm
          StarUnicode.cppm
          StarVersion.cppm
          StarZSTDCompression.cppm
          StarObserverStream.cppm
          StarPeriodic.cppm
          StarPeriodicFunction.cppm
          StarListener.cppm
          StarTtlCache.cppm
          StarLruCache.cppm
          StarWeightedPool.cppm
          StarParametricFunction.cppm
          StarMultiTable.cppm
          StarMultiArrayInterpolator.cppm
          StarSpatialHash2D.cppm
          StarSpline.cppm StarRandomPoint.cppm
          StarAStar.cppm StarEither.cppm StarRefPtr.cppm StarIdMap.cppm
          StarOutputProxy.cppm StarMemory.cppm StarNetCompatibility.cppm
          StarLexicalCast.cppm StarRandom.cppm StarByteArray.cppm
          StarBytes.cppm StarStaticRandom.cppm
          StarArray.cppm StarStaticVector.cppm StarSmallVector.cppm
          StarMultiArray.cppm StarMathCommon.cppm StarBlockAllocator.cppm
          StarAlgorithm.cppm StarPythonic.cppm
          StarVector.cppm StarMatrix3.cppm StarLine.cppm
          StarRect.cppm StarPoly.cppm StarInterpolation.cppm
          StarAudio.cppm
          StarColor.cppm
          StarFont.cppm
          StarImage.cppm
          StarInputEvent.cppm
          StarSectorArray2D.cppm)
target_sources (star_modules PRIVATE
    StarUuid.cpp StarHostAddress.cpp StarSocket.cpp StarTcp.cpp
    StarString.cpp StarStringView.cpp StarThread.cpp
    StarIODevice.cpp StarFile.cpp StarDataStream.cpp
    StarJson.cpp StarLogging.cpp StarLua.cpp
    StarWorkerPool.cpp StarOptionParser.cpp StarShellParser.cpp StarJsonPath.cpp
    StarJsonBuilder.cpp StarFormattedJson.cpp StarJsonRpc.cpp
    StarNetElement.cpp StarNetElementBasicFields.cpp
    StarNetElementGroup.cpp StarNetElementSyncGroup.cpp
    StarByteArray.cpp StarLexicalCast.cpp StarMemory.cpp
    StarNetCompatibility.cpp StarRandom.cpp
    StarAssetPath.cpp
    StarBTreeDatabase.cpp
    StarBuffer.cpp
    StarDirectives.cpp
    StarEncode.cpp
    StarImageProcessing.cpp
    StarText.cpp
    StarTime.cpp
    StarUnicode.cpp
    StarZSTDCompression.cpp
    StarListener.cpp
    StarAudio.cpp
    StarColor.cpp
    StarFont.cpp
    StarImage.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/StarVersion.cpp)
if (STAR_SYSTEM_FAMILY_UNIX)
  target_sources (star_modules PRIVATE StarLockFile_unix.cpp StarTime_unix.cpp
      StarException_unix.cpp StarFile_unix.cpp StarThread_unix.cpp)
elseif (STAR_SYSTEM_FAMILY_WINDOWS)
  target_sources (star_modules PUBLIC FILE_SET CXX_MODULES FILES StarString_windows.cppm)
  target_sources (star_modules PRIVATE StarLockFile_windows.cpp StarTime_windows.cpp
      StarException_windows.cpp StarFile_windows.cpp StarString_windows.cpp StarThread_windows.cpp)
endif ()
set_target_properties (star_modules PROPERTIES CXX_SCAN_FOR_MODULES ON)
target_include_directories (star_modules PRIVATE ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES})
target_link_libraries (star_modules PRIVATE star_build_config star_module_dependencies star_core_dependencies)
target_link_libraries (star_modules PUBLIC star_std)
target_link_libraries (star_core PRIVATE star_modules star_build_config star_core_dependencies)
# Sources that import a module need the module flags and the module dependency
# scan too; CMake only turns those on for targets that declare module sources.
set_target_properties (star_core PROPERTIES CXX_SCAN_FOR_MODULES ON)

if(STAR_COMPILER_CLANG AND STAR_SYSTEM_MACOS)
  set_source_files_properties(StarColor.cpp PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-fassociative-math;-freciprocal-math")
endif()
