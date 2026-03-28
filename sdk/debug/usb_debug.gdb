# USB debug script — run after flashing usb-serial
# Usage: make openocd (terminal 1), then:
#   arm-none-eabi-gdb build/Core-U-2-a/usb-serial/usb-serial.elf -x debug/usb_debug.gdb

target remote :3333
monitor reset halt
load

# Break at the start of USB_IRQHandler
break USB_IRQHandler
continue

# When we hit the breakpoint, dump USB state
define usb_dump
  printf "\n=== USB Register State ===\n"
  printf "USB_CNTR  = 0x%04X\n", *(uint16_t*)0x40006840
  printf "USB_ISTR  = 0x%04X\n", *(uint16_t*)0x40006844
  printf "USB_FNR   = 0x%04X\n", *(uint16_t*)0x40006848
  printf "USB_DADDR = 0x%04X\n", *(uint16_t*)0x4000684C
  printf "USB_BTABLE= 0x%04X\n", *(uint16_t*)0x40006850
  printf "USB_EP0R  = 0x%04X\n", *(uint16_t*)0x40006800
  printf "USB_EP1R  = 0x%04X\n", *(uint16_t*)0x40006804
  printf "USB_EP2R  = 0x%04X\n", *(uint16_t*)0x40006808
  printf "USB_BCDR  = 0x%04X\n", *(uint16_t*)0x40006858
  printf "\n=== PMA (first 32 bytes, raw) ===\n"
  printf "PMA+0x00 = 0x%04X  (BDT EP0 ADDR_TX if 2x16)\n", *(uint16_t*)0x40006C00
  printf "PMA+0x02 = 0x%04X\n", *(uint16_t*)0x40006C02
  printf "PMA+0x04 = 0x%04X\n", *(uint16_t*)0x40006C04
  printf "PMA+0x06 = 0x%04X\n", *(uint16_t*)0x40006C06
  printf "PMA+0x08 = 0x%04X\n", *(uint16_t*)0x40006C08
  printf "PMA+0x0A = 0x%04X\n", *(uint16_t*)0x40006C0A
  printf "PMA+0x0C = 0x%04X\n", *(uint16_t*)0x40006C0C
  printf "PMA+0x0E = 0x%04X\n", *(uint16_t*)0x40006C0E
  printf "PMA+0x10 = 0x%04X\n", *(uint16_t*)0x40006C10
  printf "PMA+0x12 = 0x%04X\n", *(uint16_t*)0x40006C12
  printf "PMA+0x14 = 0x%04X\n", *(uint16_t*)0x40006C14
  printf "PMA+0x16 = 0x%04X\n", *(uint16_t*)0x40006C16
  printf "PMA+0x18 = 0x%04X\n", *(uint16_t*)0x40006C18
  printf "PMA+0x1A = 0x%04X\n", *(uint16_t*)0x40006C1A
  printf "PMA+0x1C = 0x%04X\n", *(uint16_t*)0x40006C1C
  printf "PMA+0x1E = 0x%04X\n", *(uint16_t*)0x40006C1E
  printf "\n=== EP0R decoded ===\n"
  set $ep0r = *(uint16_t*)0x40006800
  printf "  CTR_RX   = %d\n", ($ep0r >> 15) & 1
  printf "  DTOG_RX  = %d\n", ($ep0r >> 14) & 1
  printf "  STAT_RX  = %d (%s)\n", ($ep0r >> 12) & 3, ($ep0r >> 12) & 3 == 3 ? "VALID" : ($ep0r >> 12) & 3 == 2 ? "STALL" : ($ep0r >> 12) & 3 == 1 ? "NAK" : "DISABLED"
  printf "  SETUP    = %d\n", ($ep0r >> 11) & 1
  printf "  EP_TYPE  = %d (%s)\n", ($ep0r >> 9) & 3, ($ep0r >> 9) & 3 == 1 ? "CONTROL" : "other"
  printf "  CTR_TX   = %d\n", ($ep0r >> 7) & 1
  printf "  DTOG_TX  = %d\n", ($ep0r >> 6) & 1
  printf "  STAT_TX  = %d (%s)\n", ($ep0r >> 4) & 3, ($ep0r >> 4) & 3 == 3 ? "VALID" : ($ep0r >> 4) & 3 == 2 ? "NAK" : ($ep0r >> 4) & 3 == 1 ? "STALL" : "DISABLED"
  printf "  EA       = %d\n", $ep0r & 0xF
  printf "\n=== PWR_CR2 (VDDUSB) ===\n"
  printf "PWR_CR2   = 0x%08X\n", *(uint32_t*)0x40007004
  printf "  USV bit  = %d\n", (*(uint32_t*)0x40007004 >> 10) & 1
  printf "\n=== RCC clocks ===\n"
  printf "RCC_APB1ENR1 = 0x%08X\n", *(uint32_t*)0x40021058
  printf "  USB clk  = %d\n", (*(uint32_t*)0x40021058 >> 26) & 1
  printf "  CRS clk  = %d\n", (*(uint32_t*)0x40021058 >> 24) & 1
  printf "  PWR clk  = %d\n", (*(uint32_t*)0x40021058 >> 28) & 1
  printf "RCC_CRRCR  = 0x%08X\n", *(uint32_t*)0x40021098
  printf "  HSI48ON  = %d\n", (*(uint32_t*)0x40021098 >> 0) & 1
  printf "  HSI48RDY = %d\n", (*(uint32_t*)0x40021098 >> 1) & 1
  printf "RCC_CCIPR  = 0x%08X (CLK48SEL bits 27:26)\n", *(uint32_t*)0x40021088
  printf "\n"
end

printf "\nType 'usb_dump' after the breakpoint hits to see all USB state.\n"
printf "After the reset handler runs, type 'finish' then 'usb_dump' to see post-reset state.\n\n"
