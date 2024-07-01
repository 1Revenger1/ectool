add_executable(ectool)

target_include_directories(ectool PRIVATE
	${PROJECT_SOURCE_DIR}/include
	${libusb_INCLUDE_DIRS}
)

target_compile_definitions(ectool PRIVATE
	CHROMIUM_EC
	EXTERNAL_ECTOOL_BUILD
)

target_compile_options(ectool PRIVATE
	-Wno-c99-designator
	-Wno-address-of-packed-member
	-Wno-format-security
)

target_sources(ectool PRIVATE
	ec_flash.cc
	ec_panicinfo.cc
	ectool.cc
	ectool_i2c.cc
	ectool_keyscan.cc
	misc_util.cc
	crc.cc
	comm-host.cc

	lock/gec_lock.cc
)

if(APPLE)

	target_include_directories(ectool PRIVATE
	    ${PROJECT_SOURCE_DIR}/extern/CrosECOSX/Include
	)

	target_sources(ectool PRIVATE
		comm-darwin.cc
		lock/file_lock.cc
	)

	find_library(IOKIT_LIBRARY IOKit)
	target_link_libraries(ectool PRIVATE ${IOKIT_LIBRARY})
elseif(WIN32)
	target_compile_definitions(ectool PRIVATE
		_CRT_SECURE_NO_WARNINGS
	)

	target_link_libraries(ectool PRIVATE
		getopt
		CrosECDriver
		onecoreuap_apiset.lib
	)

	if(MSVC)
		target_compile_options(ectool PRIVATE
			/FI"..\\include\\win32_shim.h"
		)
	else()
		target_compile_options(ectool PRIVATE
			-include "..\\include\\win32_shim.h"
		)
	endif()

	target_sources(ectool PRIVATE
		comm-win32.cc
		lock/win32_mutex_lock.cc
	)

	target_include_directories(ectool PRIVATE
		../include/win32
	)
else()
	target_sources(ectool PRIVATE
		comm-dev.cc
		comm-i2c.cc
		comm-lpc.cc
		comm-servo-spi.cc
		comm-usb.cc

		lock/file_lock.cc
	)
	
	target_link_libraries(ectool ${libusb_LIBRARIES} ${libftdi1_LIBRARIES})
endif()

