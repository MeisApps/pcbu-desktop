function(add_mac_app_icon TARGET)
    set(MAC_APP_ICON_SRC ${CMAKE_SOURCE_DIR}/pkg/mac/AppIcon.icon)
    set(MAC_APP_ICON_DIR ${CMAKE_CURRENT_BINARY_DIR}/AppIcon)
    set(MAC_APP_ICON_OUTPUTS ${MAC_APP_ICON_DIR}/AppIcon.icns ${MAC_APP_ICON_DIR}/Assets.car)
    file(GLOB_RECURSE MAC_APP_ICON_DEPS CONFIGURE_DEPENDS ${MAC_APP_ICON_SRC}/*)
    add_custom_command(
            OUTPUT ${MAC_APP_ICON_OUTPUTS}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MAC_APP_ICON_DIR}
            COMMAND xcrun actool ${MAC_APP_ICON_SRC}
                    --compile ${MAC_APP_ICON_DIR}
                    --output-partial-info-plist ${MAC_APP_ICON_DIR}/partial.plist
                    --app-icon AppIcon
                    --include-all-app-icons
                    --enable-on-demand-resources NO
                    --development-region en
                    --target-device mac
                    --platform macosx
                    --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
                    --output-format human-readable-text --errors --warnings
            DEPENDS ${MAC_APP_ICON_DEPS}
            COMMENT "Compiling app icon..."
    )
    target_sources(${TARGET} PRIVATE ${MAC_APP_ICON_OUTPUTS})
    set_source_files_properties(${MAC_APP_ICON_OUTPUTS} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(${TARGET} PROPERTIES
            MACOSX_BUNDLE_ICON_FILE AppIcon)
endfunction()
