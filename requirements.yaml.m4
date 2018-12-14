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
      bucket: swiftnav-artifacts-pull-requests
      prefix: piksi_firmware_private/FW_VERSION/v3
      files:
        - piksi_firmware_v3_prod.stripped.elf
    -
      bucket: swiftnav-artifacts
      prefix: piksi_fpga/NAP_VERSION
      files:
        - piksi_prod_fpga.bit

# vim: ft=yaml:
