idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS ".")

# uncomment to copy ROM partition with code, comment to only copy code
littlefs_create_partition_image(littlefs ../flash_data FLASH_IN_PROJECT)
