cmake_minimum_required(VERSION 3.28)

foreach(required IN ITEMS PAS_KIND PAS_SO PAS_SDK PAS_JAVA_HOME PAS_OUTPUT)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

set(tools "${PAS_SDK}/build-tools/36.0.0")
set(jar "${PAS_SDK}/platforms/android-37.0/android.jar")
set(manifest "${CMAKE_CURRENT_LIST_DIR}/../fixtures/android/${PAS_KIND}/AndroidManifest.xml")
set(stage "${PAS_OUTPUT}/stage-${PAS_KIND}")
set(library "libpas_${PAS_KIND}_fixture_v2.so")
file(MAKE_DIRECTORY "${stage}/lib/x86_64" "${PAS_OUTPUT}")
file(COPY_FILE "${PAS_SO}" "${stage}/lib/x86_64/${library}" ONLY_IF_DIFFERENT)

function(run_tool)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "Fixture package tool failed (${code}): ${ARGV}\n${out}\n${err}")
  endif()
endfunction()

set(unaligned "${PAS_OUTPUT}/${PAS_KIND}-unaligned.apk")
set(aligned "${PAS_OUTPUT}/${PAS_KIND}-aligned.apk")
set(final "${PAS_OUTPUT}/pas-${PAS_KIND}-fixture-v2.apk")
run_tool("${tools}/aapt2.exe" link -I "${jar}" --manifest "${manifest}" -o "${unaligned}")
execute_process(COMMAND "${tools}/aapt.exe" add "${unaligned}" "lib/x86_64/${library}"
                WORKING_DIRECTORY "${stage}" RESULT_VARIABLE add_code
                OUTPUT_VARIABLE add_out ERROR_VARIABLE add_err)
if(NOT add_code EQUAL 0)
  message(FATAL_ERROR "aapt add failed (${add_code}): ${add_out} ${add_err}")
endif()
run_tool("${tools}/zipalign.exe" -f 4 "${unaligned}" "${aligned}")
set(keystore "${PAS_OUTPUT}/debug.keystore")
if(NOT EXISTS "${keystore}")
  run_tool("${PAS_JAVA_HOME}/bin/keytool.exe" -genkeypair -noprompt
           -keystore "${keystore}" -storepass android -keypass android -alias fixture
           -dname "CN=PAS CPP Fixture" -keyalg RSA -keysize 2048 -validity 3650)
endif()
set(ENV{JAVA_HOME} "${PAS_JAVA_HOME}")
set(ENV{PAS_FIXTURE_DEBUG_PASSWORD} "android")
run_tool("${tools}/apksigner.bat" sign --ks "${keystore}" --ks-key-alias fixture
         --ks-pass env:PAS_FIXTURE_DEBUG_PASSWORD --key-pass env:PAS_FIXTURE_DEBUG_PASSWORD
         --out "${final}" "${aligned}")
run_tool("${tools}/apksigner.bat" verify "${final}")
file(SHA256 "${final}" digest)
file(WRITE "${PAS_OUTPUT}/${PAS_KIND}-artifact.json"
     "{\n  \"fixture\": \"${PAS_KIND}\",\n  \"sha256\": \"${digest}\",\n  \"apk\": \"${final}\",\n  \"ndk\": \"30.0.16248370\"\n}\n")
message(STATUS "Packaged ${final} SHA-256 ${digest}")
