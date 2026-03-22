# Changelog

## v0.6.3

### Bug Fixes

- Changed the tag for embedded flash IO from `embed_flash` to `embed`
- Refactor the script to make the generated files follow our coding style

## v0.6.2

### Features

- Limit the version of `esp_codec_dev`

## v0.6.1

- Fixed memory leaks in I/O flash destroy API
- Corrected default values for I/O file and I/O I2S elements to ensure stable initialization
- Enforced esp_gmf_err_t return type for all element initialization functions (I/O flash, HTTP)

## v0.6.0

### Features
- Added support for NULL object configuration
- Renamed component to `gmf_io`
- Updated the License

### Bug Fixes
- Standardized return values of port and data bus acquire/release APIs to esp_gmf_err_io_t only

## v0.5.1

### Bug Fixes

- Fixed the component requirements


## v0.5.0

### Features

- Initial version of `esp-gmf-io`
