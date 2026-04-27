if(CONFIG_ZMK_INPUT_PROCESSOR_PROFILE_SCALER)
  target_sources(app PRIVATE src/profile_scaler.c)
  target_include_directories(app PRIVATE include)
endif()
