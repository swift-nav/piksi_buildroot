---
-
  name: prod
  resources:
    -
      prefix: v3/prod
      files:
        - uImage.piksiv3_prod
  artifacts:
    -
      bucket: FW_S3_BUCKET
      prefix: FW_S3_PREFIX/FW_VERSION/v3
      files:
        - piksi_firmware_v3_prod.stripped.elf
    -
      bucket: swiftnav-artifacts
      prefix: piksi_fpga/NAP_VERSION
      files:
        - piksi_prod_fpga.bit

# vim: ft=yaml:
