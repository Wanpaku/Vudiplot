cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(vudiplot_library
    VERSION 1.0.0
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "Target OS: Linux")

  set(GLM_LIB "${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/glm_1.0.3")
  # Add glm library
  add_library(glm::glm SHARED IMPORTED)
  set_target_properties(glm::glm PROPERTIES
  	IMPORTED_LOCATION "${GLM_LIB}/glm/libglm.so"
	INTERFACE_INCLUDE_DIRECTORIES "${GLM_LIB}/include"
  )

  set(GETTEXT_LIB "${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/gettext_1.0")
  # Add gettext library
  add_library(gettext SHARED IMPORTED GLOBAL)
  set_target_properties(gettext PROPERTIES
	IMPORTED_LOCATION "${GETTEXT_LIB}/lib/libgettextlib.so"
	INTERFACE_INCLUDE_DIRECTORIES "${GETTEXT_LIB}/include"
  )

  find_program(XGETTEXT_EXEC
    NAMES xgettext
    PATHS ${GETTEXT_LIB}/bin
    REQUIRED
  )

    find_program(MSGMERGE_EXEC
    NAMES msgmerge
    PATHS ${GETTEXT_LIB}/bin
    REQUIRED
  )

    find_program(MSGFMT_EXEC
    NAMES msgfmt
    PATHS ${GETTEXT_LIB}/bin
    REQUIRED
  )

  set(VULKAN_LIB "${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/vulkan_1.4.363.0")
  # Add Vulkan loader
  add_library(Vulkan::Vulkan SHARED IMPORTED GLOBAL)
  set_target_properties(Vulkan::Vulkan PROPERTIES
	IMPORTED_LOCATION "${VULKAN_LIB}/VulkanLoader/lib/libvulkan.so.1.4.363"
	INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_LIB}/include"
  )

  set(SDL3_LIB "${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/SDL3-3.4.18_shared")
  # Add SDL3 libary
  add_library(SDL3::SDL3 SHARED IMPORTED GLOBAL)
  set_target_properties(SDL3::SDL3 PROPERTIES
	IMPORTED_LOCATION "${SDL3_LIB}/libSDL3.so"
	INTERFACE_INCLUDE_DIRECTORIES "${SDL3_LIB}/include"
  )

  # Add KTX library
  set(KTX_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/KTX-Software-5.0.0-rc2-Linux-x86_64/include")
  set(KTX_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/KTX-Software-5.0.0-rc2-Linux-x86_64/lib")
  add_library(KTX::ktx SHARED IMPORTED GLOBAL)
  set_target_properties(KTX::ktx PROPERTIES
	IMPORTED_LOCATION "${KTX_LIBRARY_DIR}/libktx.so.5.0.0"
	INTERFACE_INCLUDE_DIRECTORIES "${KTX_INCLUDE_DIR}"
  )

  # Attempt to find the slang compiler (slangc)
  find_program(SLANGC_EXECUTABLE
    NAMES slangc
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/slangc/bin
    REQUIRED
  )


elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Target OS: Windows")
endif()


set(IMGUI_LIBRARY_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/imgui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/imgui_draw.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/imgui_widgets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/imgui_tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/imgui_demo.cpp
)

set(IMPLOT_LIBRARY_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/implot.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/implot_demo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library/implot_items.cpp
)

set(LODEPNG_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/lodepng/lodepng.cpp
)

set(IMGUI_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/imgui_library
)

set(LODEPNG_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/libs/Linux/lodepng
)

set(VULKAN_COMPONENTS
  ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan_components/GenInstance/GenInstance.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan_components/GenCommandBuffer/GenCommandBuffer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan_components/GenPresentation/GenPresentation.cpp
)

set(CUSTOM_UTILS
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/BufferUtils/BufferUtils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/GenSDLWindow/GenSDLWindow.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/GenGettext/GenGettext.cpp
)

set(GUI
  ${CMAKE_CURRENT_SOURCE_DIR}/GUI/ImGuiVulkanUtil/ImGuiVulkanUtil.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GUI/ImGuiTextureManager/ImGuiTextureManager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GUI/ImGuiTextureMesh/ImGuiTextureMesh.cpp
)

set(VUDIPLOT_API
  ${CMAKE_CURRENT_SOURCE_DIR}/Vudiplot/Vudiplot.cpp
)

# Function for processing slang shaders
function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders)
  set(SHADER_OUTPUTS)

  foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
	get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)

    string(REGEX REPLACE "\\.slang$" ".spv" SHADER_OUTPUT_NAME "${SHADER_NAME}")

    set(SHADER_OUTPUT
      ${SHADERS_DIR}/${SHADER_OUTPUT_NAME}
    )
	set (ENTRY_POINTS -entry vert_main -entry frag_main)
	
	add_custom_command (
	  OUTPUT ${SHADER_OUTPUT}
	  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
	  COMMAND ${SLANGC_EXECUTABLE}
	          ${SHADER_SOURCE}
			  -target spirv
			  -profile spirv_1_4
			  -emit-spirv-directly
			  -fvk-use-entrypoint-name
			  ${ENTRY_POINTS}
			  -o ${SHADER_OUTPUT}
	  DEPENDS ${SHADER_SOURCE}
	  COMMENT "Compiling ${SHADER_NAME} -> ${SHADER_OUTPUT_NAME}"
	  VERBATIM
	)
	list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
  endforeach()
  add_custom_target (${TARGET} DEPENDS ${SHADER_OUTPUTS})
endfunction()


# gettext configuration

# Translation settings
set(DOMAIN vudiplot)
set(LOCALE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources/locales)
set(PO_DIR ${LOCALE_DIR}/po)
set(POT_FILE ${PO_DIR}/${DOMAIN}.pot)
set(TRANSLATION_SOURCES
  ${GUI}
  ${CUSTOM_UTILS}
  ${VULKAN_COMPONENTS}
  ${VUDIPLOT_API}
)

# List of languages
set(LANGUAGES uk en)

# Extract strings for translation
add_custom_command(
    OUTPUT ${POT_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PO_DIR}
    COMMAND ${XGETTEXT_EXEC}
            --keyword=gettext
            --language=C++
            --output=${POT_FILE}
            ${TRANSLATION_SOURCES}
    DEPENDS ${TRANSLATION_SOURCES}
    COMMENT "Extracting translatable strings"
)

set(MO_FILES)

foreach(LANG ${LANGUAGES})
    set(PO_FILE ${PO_DIR}/${LANG}.po)
    set(MO_FILE ${LOCALE_DIR}/${LANG}/LC_MESSAGES/${DOMAIN}.mo)

    # Create initial PO file if it doesn't exist, then merge POT into it.
    add_custom_command(
        OUTPUT ${PO_FILE}
        COMMAND ${CMAKE_COMMAND} -E touch ${PO_FILE}
        COMMAND ${MSGMERGE_EXEC}
                --update
                --backup=none
                ${PO_FILE}
                ${POT_FILE}
        DEPENDS ${POT_FILE}
        COMMENT "Updating ${LANG}.po"
    )

    add_custom_command(
        OUTPUT ${MO_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${LOCALE_DIR}/${LANG}/LC_MESSAGES
        COMMAND ${MSGFMT_EXEC}
                ${PO_FILE}
                --output-file=${MO_FILE}
        DEPENDS ${PO_FILE} ${POT_FILE}
        COMMENT "Compiling ${LANG}.po"
    )

    list(APPEND MO_FILES ${MO_FILE})
endforeach()

add_custom_target(translations ALL
    DEPENDS ${MO_FILES}
)

add_slang_shader_target(shaders SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/imgui.slang
)

set(VUDIPLOT_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan_components/GenInstance
  ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan_components/GenCommandBuffer
  ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan_components/GenPresentation
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/BufferUtils
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/InitUtils
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/GenSDLWindow
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/GenGettext
  ${CMAKE_CURRENT_SOURCE_DIR}/GUI/ImGuiVulkanUtil
  ${CMAKE_CURRENT_SOURCE_DIR}/GUI/ImGuiTextureManager
  ${CMAKE_CURRENT_SOURCE_DIR}/GUI/ImGuiTextureMesh
  ${CMAKE_CURRENT_SOURCE_DIR}/Vudiplot
)

add_library(vudiplot SHARED
  ${VULKAN_COMPONENTS}
  ${CUSTOM_UTILS}
  ${GUI}
  ${IMGUI_LIBRARY_SOURCES}
  ${IMPLOT_LIBRARY_SOURCES}
  ${LODEPNG_SOURCES}
  ${VUDIPLOT_API}
)

target_include_directories(vudiplot
  PRIVATE
  ${VUDIPLOT_INCLUDES}
  ${LODEPNG_INCLUDES}
)

target_include_directories(vudiplot PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Vudiplot
  ${CMAKE_CURRENT_SOURCE_DIR}/Utils/InitUtils
  ${IMGUI_INCLUDES}
)


target_link_libraries(vudiplot
    PRIVATE
    Vulkan::Vulkan
    glm::glm
    SDL3::SDL3
    KTX::ktx
	gettext
  )

target_compile_definitions(vudiplot PRIVATE
    RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

set(VUDIPLOT_DEPENDENCIES_BUILD_PATH
  "$ORIGIN/libs/Linux/glm_1.0.3/glm"
  "$ORIGIN/libs/Linux/gettext_1.0/lib"
  "$ORIGIN/libs/Linux/vulkan_1.4.363.0/VulkanLoader/lib"
  "$ORIGIN/libs/Linux/SDL3-3.4.18_shared"
  "$ORIGIN/libs/Linux/KTX-Software-5.0.0-rc2-Linux-x86_64/lib"
)

set_target_properties(vudiplot PROPERTIES
  BUILD_RPATH "${VUDIPLOT_DEPENDENCIES_BUILD_PATH}"
  INSTALL_RPATH "${VUDIPLOT_DEPENDENCIES_BUILD_PATH}"
  BUILD_RPATH_USE_ORIGIN TRUE
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH_USE_LINK_PATH FALSE
)

add_custom_target(copy_dependencies
  
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/libs"
    "$<TARGET_FILE_DIR:vudiplot>/libs"

	COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:vudiplot>/resources"

	COMMENT "Copying third party libraries and resources for vudiplot"
  
)
add_dependencies(copy_dependencies translations shaders)
add_dependencies(vudiplot copy_dependencies)

option(ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

if(ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -g
    )

    add_link_options(
        -fsanitize=address,undefined
    )
endif()


set(ENABLE_TESTING FALSE)
    
if(ENABLE_TESTING)
    include(CTest)

    find_package(Catch2 3 REQUIRED)
	find_program(GCOVR gcovr REQUIRED)

    add_executable(tests
	  tests/unit_tests.cpp
	  tests/integration_tests.cpp
	  ${VULKAN_COMPONENTS}
	  ${CUSTOM_UTILS}
	  ${GUI}
	  ${IMGUI_LIBRARY_SOURCES}
	  ${IMPLOT_LIBRARY_SOURCES}
	  ${LODEPNG_SOURCES}
	  ${VUDIPLOT_API}
    )

	target_compile_definitions(tests PRIVATE
      RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
	)

	target_include_directories(tests PRIVATE
	  ${VUDIPLOT_INCLUDES}
	  ${IMGUI_INCLUDES}
	  ${LODEPNG_INCLUDES}
	)


    target_link_libraries(tests
      PRIVATE Catch2::Catch2WithMain
  	    Vulkan::Vulkan
		glm::glm
		SDL3::SDL3
		KTX::ktx
		gettext
    )

	target_compile_options(vudiplot PRIVATE
		--coverage
		-g
		-O0
		-fPIC
      )

    target_link_options(vudiplot PRIVATE
	    --coverage
      )

	target_compile_options(tests PRIVATE
	    --coverage
	    -g
	    -O0
	    -fPIC
      )

    target_link_options(tests PRIVATE
	    --coverage
      )

	
    include(Catch)
    catch_discover_tests(tests)
	
	add_custom_target(tests_with_coverage ALL

      COMMAND ${CMAKE_CTEST_COMMAND}
      --test-dir ${CMAKE_CURRENT_BINARY_DIR}
      --output-on-failure
	  --stop-on-failure

      COMMAND ${GCOVR}
      --root "${CMAKE_CURRENT_SOURCE_DIR}"
      --gcov-executable "gcov"
	  --gcov-delete			
	  --merge-mode-functions merge-use-line-0
	  --exclude ".*libs/.*"
	  --exclude ".*resources/.*"
	  --txt
	  --sort uncovered-percent
      --print-summary
	  --html-details "vudiplot_tests_coverage.html"
	  --html-single-page
	  
      DEPENDS vudiplot tests 
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  )

 # add_dependencies(tests_with_coverage vudiplot)

endif()

if(CMAKE_CXX_COMPILER_LOADED)
     message(STATUS "Compiler path: ${CMAKE_CXX_COMPILER}")
     message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
     message(STATUS "Compiler version:
             ${CMAKE_CXX_COMPILER_VERSION}")
	 message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
endif()
