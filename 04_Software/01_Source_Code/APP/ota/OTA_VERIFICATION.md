# OTA verification checklist (single APP1 + dual external packages)

Manufacturing baseline (required):

- External **SLOT_A** at `OTA_NEW_FW_ADDR` contains a valid full package (`Image_Info_t` + raw bin linked at `APP1_FLASH_ADDR`).
- At least one flag mirror (`OTA_FLAG1_ADDR` / `OTA_FLAG2_ADDR`) is programmed: `committed_slot=SLOT_A`, `staging_slot=NONE`, `ota_state=IDLE`, sane `seq`, `OTA_FLAG_MAGIC`.

Functional tests:

1. **Normal upgrade**: USB `0:/app.bin` → full package programmed to staging slot → device resets (`HAL_NVIC_SystemReset`) → bootloader installs payload to APP1 (`OTA_DOWNLOADED`) → bootloader sets `OTA_INSTALLED`; APP waits 20s then confirms (`OTA_IDLE`, staging promoted to `committed_slot`).
2. **Reject bad images**: Wrong CRC32, MSP, or Thumb `Reset_Handler` not in APP1 range must abort (`app` pre-check / `bootloader` package validation).
3. **Rollback**: After install, reboot before APP confirm until `OTA_TRIAL_BOOT_MAX` (3 bootloader passes with `OTA_INSTALLED`) ⇒ reinstall `committed_slot` package onto APP1 → `OTA_IDLE`.

Power-loss probes (observe UART logs):

- Staging interruption may leave FAILED or mismatched staging; retry with a fresh USB image after recovery.
