/* Automatically generated file; DO NOT EDIT */
/* Espressif IoT Development Framework Linker Script */
/* Generated from: /home/jakobsen/esp/esp-idf/components/esp32/ld/esp32.project.ld.in */

/*  Default entry point:  */
ENTRY(call_start_cpu0);

SECTIONS
{
  /* RTC fast memory holds RTC wake stub code,
     including from any source file named rtc_wake_stub*.c
  */
  .rtc.text :
  {
    . = ALIGN(4);

    *(EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.literal EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.text EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.text.*)
    *libsoc.a:uart_hal_iram.*( .rtc.literal  .rtc.text  .rtc.text.*)

    *rtc_wake_stub*.*(.literal .text .literal.* .text.*)
    _rtc_text_end = ABSOLUTE(.);
  } > rtc_iram_seg

  /*
    This section is required to skip rtc.text area because rtc_iram_seg and
    rtc_data_seg are reflect the same address space on different buses.
  */
  .rtc.dummy :
  {
    _rtc_dummy_start = ABSOLUTE(.);
    _rtc_fast_start = ABSOLUTE(.);
    . = SIZEOF(.rtc.text);
    _rtc_dummy_end = ABSOLUTE(.);
  } > rtc_data_seg

  /* This section located in RTC FAST Memory area.
     It holds data marked with RTC_FAST_ATTR attribute.
     See the file "esp_attr.h" for more information.
  */
  .rtc.force_fast :
  {
    . = ALIGN(4);
    _rtc_force_fast_start = ABSOLUTE(.);
    *(.rtc.force_fast .rtc.force_fast.*)
    . = ALIGN(4) ;
    _rtc_force_fast_end = ABSOLUTE(.);
  } > rtc_data_seg

  /* RTC data section holds RTC wake stub
     data/rodata, including from any source file
     named rtc_wake_stub*.c and the data marked with
     RTC_DATA_ATTR, RTC_RODATA_ATTR attributes.
     The memory location of the data is dependent on
     CONFIG_ESP32_RTCDATA_IN_FAST_MEM option.
  */
  .rtc.data :
  {
    _rtc_data_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.data EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.data.* EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.rodata EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.rodata.*)
    *libsoc.a:uart_hal_iram.*( .rtc.data  .rtc.data.*  .rtc.rodata  .rtc.rodata.*)

    *rtc_wake_stub*.*(.data .rodata .data.* .rodata.* .bss .bss.*)
    _rtc_data_end = ABSOLUTE(.);
  } > rtc_data_location

  /* RTC bss, from any source file named rtc_wake_stub*.c */
  .rtc.bss (NOLOAD) :
  {
    _rtc_bss_start = ABSOLUTE(.);
    *rtc_wake_stub*.*(.bss .bss.*)
    *rtc_wake_stub*.*(COMMON)

    *(EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .rtc.bss)
    *libsoc.a:uart_hal_iram.*( .rtc.bss)

    _rtc_bss_end = ABSOLUTE(.);
  } > rtc_data_location

  /* This section holds data that should not be initialized at power up
     and will be retained during deep sleep.
     User data marked with RTC_NOINIT_ATTR will be placed
     into this section. See the file "esp_attr.h" for more information.
	 The memory location of the data is dependent on
     CONFIG_ESP32_RTCDATA_IN_FAST_MEM option.
  */
  .rtc_noinit (NOLOAD):
  {
    . = ALIGN(4);
    _rtc_noinit_start = ABSOLUTE(.);
    *(.rtc_noinit .rtc_noinit.*)
    . = ALIGN(4) ;
    _rtc_noinit_end = ABSOLUTE(.);
  } > rtc_data_location

  /* This section located in RTC SLOW Memory area.
     It holds data marked with RTC_SLOW_ATTR attribute.
     See the file "esp_attr.h" for more information.
  */
  .rtc.force_slow :
  {
    . = ALIGN(4);
    _rtc_force_slow_start = ABSOLUTE(.);
    *(.rtc.force_slow .rtc.force_slow.*)
    . = ALIGN(4) ;
    _rtc_force_slow_end = ABSOLUTE(.);
  } > rtc_slow_seg

  /* Get size of rtc slow data based on rtc_data_location alias */
  _rtc_slow_length = (ORIGIN(rtc_slow_seg) == ORIGIN(rtc_data_location))
                        ? (_rtc_force_slow_end - _rtc_data_start)
                        : (_rtc_force_slow_end - _rtc_force_slow_start);

  _rtc_fast_length = (ORIGIN(rtc_slow_seg) == ORIGIN(rtc_data_location))
                        ? (_rtc_force_fast_end - _rtc_fast_start)
                        : (_rtc_noinit_end - _rtc_fast_start);

  ASSERT((_rtc_slow_length <= LENGTH(rtc_slow_seg)),
          "RTC_SLOW segment data does not fit.")

  ASSERT((_rtc_fast_length <= LENGTH(rtc_data_seg)),
          "RTC_FAST segment data does not fit.")

  /* Send .iram0 code to iram */
  .iram0.vectors :
  {
    _iram_start = ABSOLUTE(.);
    /* Vectors go to IRAM */
    _init_start = ABSOLUTE(.);
    /* Vectors according to builds/RF-2015.2-win32/esp108_v1_2_s5_512int_2/config.html */
    . = 0x0;
    KEEP(*(.WindowVectors.text));
    . = 0x180;
    KEEP(*(.Level2InterruptVector.text));
    . = 0x1c0;
    KEEP(*(.Level3InterruptVector.text));
    . = 0x200;
    KEEP(*(.Level4InterruptVector.text));
    . = 0x240;
    KEEP(*(.Level5InterruptVector.text));
    . = 0x280;
    KEEP(*(.DebugExceptionVector.text));
    . = 0x2c0;
    KEEP(*(.NMIExceptionVector.text));
    . = 0x300;
    KEEP(*(.KernelExceptionVector.text));
    . = 0x340;
    KEEP(*(.UserExceptionVector.text));
    . = 0x3C0;
    KEEP(*(.DoubleExceptionVector.text));
    . = 0x400;
    *(.*Vector.literal)

    *(.UserEnter.literal);
    *(.UserEnter.text);
    . = ALIGN (16);
    *(.entry.text)
    *(.init.literal)
    *(.init)
    _init_end = ABSOLUTE(.);
  } > iram0_0_seg

  .iram0.text :
  {
    /* Code marked as runnning out of IRAM */
    _iram_text_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .iram1 EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .iram1.*)
    *libesp_ringbuf.a:( .literal  .literal.*  .text  .text.*)
    *libgcov.a:( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:app_trace_util.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:SEGGER_RTT_esp32.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:app_trace.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:SEGGER_SYSVIEW.*( .literal  .literal.*  .text  .text.*)
    *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.*( .literal  .literal.*  .text  .text.*)
    *liblog.a:log_freertos.*(.literal.esp_log_early_timestamp .text.esp_log_early_timestamp)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock_timeout .text.esp_log_impl_lock_timeout)
    *liblog.a:log_freertos.*(.literal.esp_log_timestamp .text.esp_log_timestamp)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_unlock .text.esp_log_impl_unlock)
    *liblog.a:log.*(.literal.esp_log_write .text.esp_log_write)
    *liblog.a:log_freertos.*(.literal.esp_log_impl_lock .text.esp_log_impl_lock)
    *libpp.a:( .wifirxiram  .wifirxiram.*)
    *libpp.a:( .wifi0iram  .wifi0iram.*)
    *libgcc.a:_divsf3.*( .literal  .literal.*  .text  .text.*)
    *libgcc.a:lib2funcs.*( .literal  .literal.*  .text  .text.*)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_isr_post .text.esp_event_isr_post)
    *libesp_event.a:esp_event.*(.literal.esp_event_isr_post_to .text.esp_event_isr_post_to)
    *librtc.a:( .literal  .literal.*  .text  .text.*)
    *libheap.a:multi_heap.*( .literal  .literal.*  .text  .text.*)
    *libheap.a:multi_heap_poisoning.*( .literal  .literal.*  .text  .text.*)
    *libnet80211.a:( .wifirxiram  .wifirxiram.*)
    *libnet80211.a:( .wifi0iram  .wifi0iram.*)
    *libesp32.a:panic.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:memspi_host_driver.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_chip_generic.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_rom_patch.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_chip_gd.*( .literal  .literal.*  .text  .text.*)
    *libspi_flash.a:spi_flash_chip_issi.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_clk.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:i2c_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_wdt.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:spi_flash_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_pm.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:spi_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:spi_slave_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:cpu_util.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:ledc_hal_iram.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:uart_hal_iram.*( .iram1  .iram1.*)
    *libsoc.a:spi_flash_hal_gpspi.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_periph.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:lldesc.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_init.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_sleep.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_clk_init.*( .literal  .literal.*  .text  .text.*)
    *libsoc.a:rtc_time.*( .literal  .literal.*  .text  .text.*)
    *libxtensa.a:eri.*( .literal  .literal.*  .text  .text.*)
    *libnewlib.a:heap.*( .literal  .literal.*  .text  .text.*)
    *libhal.a:( .literal  .literal.*  .text  .text.*)
    *libfreertos.a:( .literal  .literal.*  .text  .text.*)

    _iram_text_end = ABSOLUTE(.);
  } > iram0_0_seg

  .dram0.data :
  {
    _data_start = ABSOLUTE(.);
    _bt_data_start = ABSOLUTE(.);
    *libbt.a:(.data .data.*)
    . = ALIGN (4);
    _bt_data_end = ABSOLUTE(.);
    _btdm_data_start = ABSOLUTE(.);
    *libbtdm_app.a:(.data .data.*)
    . = ALIGN (4);
    _btdm_data_end = ABSOLUTE(.);
    _nimble_data_start = ABSOLUTE(.);
    *libnimble.a:(.data .data.*)
    . = ALIGN (4);
    _nimble_data_end = ABSOLUTE(.);
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)


    *(EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .data EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .data.* EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .dram1 EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .dram1.*)
    *libgcov.a:( .rodata  .rodata.*)
    *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.*( .rodata  .rodata.*)
    *libapp_trace.a:app_trace_util.*( .rodata  .rodata.*)
    *libapp_trace.a:SEGGER_RTT_esp32.*( .rodata  .rodata.*)
    *libapp_trace.a:app_trace.*( .rodata  .rodata.*)
    *libapp_trace.a:SEGGER_SYSVIEW.*( .rodata  .rodata.*)
    *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.*( .rodata  .rodata.*)
    *liblog.a:log_freertos.*(.rodata.esp_log_early_timestamp)
    *liblog.a:log_freertos.*(.rodata.esp_log_impl_lock_timeout)
    *liblog.a:log_freertos.*(.rodata.esp_log_timestamp)
    *liblog.a:log_freertos.*(.rodata.esp_log_impl_unlock)
    *liblog.a:log.*(.rodata.esp_log_write)
    *liblog.a:log_freertos.*(.rodata.esp_log_impl_lock)
    *libgcc.a:_divsf3.*( .rodata  .rodata.*)
    *libesp_event.a:default_event_loop.*(.rodata.esp_event_isr_post)
    *libesp_event.a:esp_event.*(.rodata.esp_event_isr_post_to)
    *libheap.a:multi_heap.*( .rodata  .rodata.*)
    *libheap.a:multi_heap_poisoning.*( .rodata  .rodata.*)
    *libesp32.a:panic.*( .rodata  .rodata.*)
    *libspi_flash.a:memspi_host_driver.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_chip_generic.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_chip_gd.*( .rodata  .rodata.*)
    *libspi_flash.a:spi_flash_chip_issi.*( .rodata  .rodata.*)
    *libsoc.a:rtc_clk.*( .rodata  .rodata.*)
    *libsoc.a:i2c_hal_iram.*( .rodata  .rodata.*)
    *libsoc.a:spi_flash_hal_iram.*( .rodata  .rodata.*)
    *libsoc.a:uart_hal_iram.*( .data  .data.*  .dram1  .dram1.*)
    *libsoc.a:spi_flash_hal_gpspi.*( .rodata  .rodata.*)
    *libnewlib.a:heap.*( .rodata  .rodata.*)
    *libphy.a:( .rodata  .rodata.*)

    _data_end = ABSOLUTE(.);
    . = ALIGN(4);
  } > dram0_0_seg

  /*This section holds data that should not be initialized at power up.
    The section located in Internal SRAM memory region. The macro _NOINIT
    can be used as attribute to place data into this section.
    See the esp_attr.h file for more information.
  */
  .noinit (NOLOAD):
  {
    . = ALIGN(4);
    _noinit_start = ABSOLUTE(.);
    *(.noinit .noinit.*)
    . = ALIGN(4) ;
    _noinit_end = ABSOLUTE(.);
  } > dram0_0_seg

  /* Shared RAM */
  .dram0.bss (NOLOAD) :
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.ext_ram.bss*)
    _bt_bss_start = ABSOLUTE(.);
    *libbt.a:(.bss .bss.* COMMON)
    . = ALIGN (4);
    _bt_bss_end = ABSOLUTE(.);
    _btdm_bss_start = ABSOLUTE(.);
    *libbtdm_app.a:(.bss .bss.* COMMON)
    . = ALIGN (4);
    _btdm_bss_end = ABSOLUTE(.);
    _nimble_bss_start = ABSOLUTE(.);
    *libnimble.a:(.bss .bss.* COMMON)
    . = ALIGN (4);
    _nimble_bss_end = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .bss EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) .bss.* EXCLUDE_FILE(*libsoc.a:uart_hal_iram.*) COMMON)
    *libsoc.a:uart_hal_iram.*( .bss  .bss.*  COMMON)

    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.share.mem)
    *(.gnu.linkonce.b.*)

    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } > dram0_0_seg

  ASSERT(((_bss_end - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")

  /* When modifying the alignment, update tls_section_alignment in pxPortInitialiseStack */
  .flash.rodata : ALIGN(0x10)
  {
    _rodata_start = ABSOLUTE(.);

    *(.rodata_desc .rodata_desc.*)               /* Should be the first.  App version info.        DO NOT PUT ANYTHING BEFORE IT! */
    *(.rodata_custom_desc .rodata_custom_desc.*) /* Should be the second. Custom app version info. DO NOT PUT ANYTHING BEFORE IT! */

    *(EXCLUDE_FILE(*libgcov.a *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libgcc.a:_divsf3.* *libheap.a:multi_heap_poisoning.* *libheap.a:multi_heap.* *libesp32.a:panic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:memspi_host_driver.* *libsoc.a:spi_flash_hal_gpspi.* *libsoc.a:uart_hal_iram.* *libsoc.a:spi_flash_hal_iram.* *libsoc.a:i2c_hal_iram.* *libsoc.a:rtc_clk.* *libnewlib.a:heap.* *libphy.a) .rodata EXCLUDE_FILE(*libgcov.a *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *liblog.a:log_freertos.* *liblog.a:log.* *libgcc.a:_divsf3.* *libesp_event.a:esp_event.* *libesp_event.a:default_event_loop.* *libheap.a:multi_heap_poisoning.* *libheap.a:multi_heap.* *libesp32.a:panic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:memspi_host_driver.* *libsoc.a:spi_flash_hal_gpspi.* *libsoc.a:uart_hal_iram.* *libsoc.a:spi_flash_hal_iram.* *libsoc.a:i2c_hal_iram.* *libsoc.a:rtc_clk.* *libnewlib.a:heap.* *libphy.a) .rodata.*)
    *liblog.a:log.*(.rodata.esp_log_level_set.str1.4 .rodata.__func__$3523 .rodata.__func__$3500)
    *liblog.a:log_freertos.*(.rodata.esp_log_system_timestamp.str1.4)
    *libesp_event.a:default_event_loop.*(.rodata.esp_event_loop_create_default.str1.4 .rodata.esp_event_send_to_default_loop)
    *libesp_event.a:esp_event.*(.rodata.handler_instances_add.str1.4 .rodata.base_node_add_handler.str1.4 .rodata.loop_node_add_handler.str1.4 .rodata.esp_event_loop_create.str1.4 .rodata.esp_event_loop_run.str1.4 .rodata.esp_event_loop_run_task.str1.4 .rodata.esp_event_handler_register_with_internal.str1.4 .rodata.esp_event_handler_unregister_with_internal.str1.4 .rodata.__func__$8822 .rodata.__func__$8809 .rodata.__func__$8776 .rodata.__func__$8744 .rodata.__func__$8719 .rodata.__func__$8678 .rodata.__func__$8669)
    *libsoc.a:uart_hal_iram.*( .rodata  .rodata.*)

    *(.irom1.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE_ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table .gcc_except_table.*)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    . = (. + 3) & ~ 3;
    __eh_frame = ABSOLUTE(.);
    KEEP(*(.eh_frame))
    . = (. + 7) & ~ 3;
    /*  C++ constructor and destructor tables

        Make a point of not including anything from crtbegin.o or crtend.o, as IDF doesn't use toolchain crt
      */
    __init_array_start = ABSOLUTE(.);
    KEEP (*(EXCLUDE_FILE (*crtend.* *crtbegin.*) .ctors .ctors.*))
    __init_array_end = ABSOLUTE(.);
    KEEP (*crtbegin.*(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.*) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS_ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    /* Addresses of memory regions reserved via
       SOC_RESERVE_MEMORY_REGION() */
    soc_reserved_memory_region_start = ABSOLUTE(.);
    KEEP (*(.reserved_memory_address))
    soc_reserved_memory_region_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
    /* Literals are also RO data. */
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
    . = ALIGN(4);
    _thread_local_start = ABSOLUTE(.);
    *(.tdata)
    *(.tdata.*)
    *(.tbss)
    *(.tbss.*)
    _thread_local_end = ABSOLUTE(.);
    . = ALIGN(4);
  } >default_rodata_seg

  .flash.text :
  {
    _stext = .;
    _text_start = ABSOLUTE(.);

    *(EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *librtc.a *libheap.a:multi_heap_poisoning.* *libheap.a:multi_heap.* *libesp32.a:panic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:memspi_host_driver.* *libsoc.a:rtc_time.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_init.* *libsoc.a:lldesc.* *libsoc.a:rtc_periph.* *libsoc.a:spi_flash_hal_gpspi.* *libsoc.a:uart_hal_iram.* *libsoc.a:ledc_hal_iram.* *libsoc.a:cpu_util.* *libsoc.a:spi_slave_hal_iram.* *libsoc.a:spi_hal_iram.* *libsoc.a:rtc_pm.* *libsoc.a:spi_flash_hal_iram.* *libsoc.a:rtc_wdt.* *libsoc.a:i2c_hal_iram.* *libsoc.a:rtc_clk.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libhal.a *libfreertos.a) .literal EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *liblog.a:log.* *liblog.a:log_freertos.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *librtc.a *libheap.a:multi_heap_poisoning.* *libheap.a:multi_heap.* *libesp32.a:panic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:memspi_host_driver.* *libsoc.a:rtc_time.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_init.* *libsoc.a:lldesc.* *libsoc.a:rtc_periph.* *libsoc.a:spi_flash_hal_gpspi.* *libsoc.a:uart_hal_iram.* *libsoc.a:ledc_hal_iram.* *libsoc.a:cpu_util.* *libsoc.a:spi_slave_hal_iram.* *libsoc.a:spi_hal_iram.* *libsoc.a:rtc_pm.* *libsoc.a:spi_flash_hal_iram.* *libsoc.a:rtc_wdt.* *libsoc.a:i2c_hal_iram.* *libsoc.a:rtc_clk.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libhal.a *libfreertos.a) .literal.* EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *librtc.a *libheap.a:multi_heap_poisoning.* *libheap.a:multi_heap.* *libesp32.a:panic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:memspi_host_driver.* *libsoc.a:rtc_time.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_init.* *libsoc.a:lldesc.* *libsoc.a:rtc_periph.* *libsoc.a:spi_flash_hal_gpspi.* *libsoc.a:uart_hal_iram.* *libsoc.a:ledc_hal_iram.* *libsoc.a:cpu_util.* *libsoc.a:spi_slave_hal_iram.* *libsoc.a:spi_hal_iram.* *libsoc.a:rtc_pm.* *libsoc.a:spi_flash_hal_iram.* *libsoc.a:rtc_wdt.* *libsoc.a:i2c_hal_iram.* *libsoc.a:rtc_clk.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libhal.a *libfreertos.a) .text EXCLUDE_FILE(*libesp_ringbuf.a *libgcov.a *libapp_trace.a:SEGGER_SYSVIEW_Config_FreeRTOS.* *libapp_trace.a:SEGGER_SYSVIEW.* *libapp_trace.a:app_trace.* *libapp_trace.a:SEGGER_RTT_esp32.* *libapp_trace.a:app_trace_util.* *libapp_trace.a:SEGGER_SYSVIEW_FreeRTOS.* *liblog.a:log.* *liblog.a:log_freertos.* *libgcc.a:lib2funcs.* *libgcc.a:_divsf3.* *libesp_event.a:default_event_loop.* *libesp_event.a:esp_event.* *librtc.a *libheap.a:multi_heap_poisoning.* *libheap.a:multi_heap.* *libesp32.a:panic.* *libspi_flash.a:spi_flash_chip_issi.* *libspi_flash.a:spi_flash_chip_gd.* *libspi_flash.a:spi_flash_rom_patch.* *libspi_flash.a:spi_flash_chip_generic.* *libspi_flash.a:memspi_host_driver.* *libsoc.a:rtc_time.* *libsoc.a:rtc_clk_init.* *libsoc.a:rtc_sleep.* *libsoc.a:rtc_init.* *libsoc.a:lldesc.* *libsoc.a:rtc_periph.* *libsoc.a:spi_flash_hal_gpspi.* *libsoc.a:uart_hal_iram.* *libsoc.a:ledc_hal_iram.* *libsoc.a:cpu_util.* *libsoc.a:spi_slave_hal_iram.* *libsoc.a:spi_hal_iram.* *libsoc.a:rtc_pm.* *libsoc.a:spi_flash_hal_iram.* *libsoc.a:rtc_wdt.* *libsoc.a:i2c_hal_iram.* *libsoc.a:rtc_clk.* *libxtensa.a:eri.* *libnewlib.a:heap.* *libhal.a *libfreertos.a) .text.* EXCLUDE_FILE(*libpp.a *libnet80211.a *libsoc.a:uart_hal_iram.*) .wifi0iram EXCLUDE_FILE(*libpp.a *libnet80211.a *libsoc.a:uart_hal_iram.*) .wifi0iram.* EXCLUDE_FILE(*libpp.a *libnet80211.a *libsoc.a:uart_hal_iram.*) .wifirxiram EXCLUDE_FILE(*libpp.a *libnet80211.a *libsoc.a:uart_hal_iram.*) .wifirxiram.*)
    *liblog.a:log_freertos.*(.literal.esp_log_system_timestamp .text.esp_log_system_timestamp)
    *liblog.a:log.*(.literal.heap_bubble_down .literal.esp_log_set_vprintf .literal.esp_log_level_set .text.heap_bubble_down .text.esp_log_set_vprintf .text.esp_log_level_set)
    *libesp_event.a:esp_event.*(.literal.handler_instances_remove_all .literal.base_node_remove_all_handler .literal.loop_node_remove_all_handler .literal.handler_instances_add .literal.base_node_add_handler .literal.loop_node_add_handler .literal.handler_instances_remove .literal.base_node_remove_handler .literal.loop_node_remove_handler .literal.esp_event_loop_create .literal.esp_event_loop_run .literal.esp_event_loop_run_task .literal.esp_event_loop_delete .literal.esp_event_handler_register_with_internal .literal.esp_event_handler_register_with .literal.esp_event_handler_instance_register_with .literal.esp_event_handler_unregister_with_internal .literal.esp_event_handler_unregister_with .literal.esp_event_handler_instance_unregister_with .literal.esp_event_post_to .text.handler_execute .text.handler_instances_remove_all .text.base_node_remove_all_handler .text.loop_node_remove_all_handler .text.handler_instances_add .text.base_node_add_handler .text.loop_node_add_handler .text.handler_instances_remove .text.base_node_remove_handler .text.loop_node_remove_handler .text.esp_event_loop_create .text.esp_event_loop_run .text.esp_event_loop_run_task .text.esp_event_loop_delete .text.esp_event_handler_register_with_internal .text.esp_event_handler_register_with .text.esp_event_handler_instance_register_with .text.esp_event_handler_unregister_with_internal .text.esp_event_handler_unregister_with .text.esp_event_handler_instance_unregister_with .text.esp_event_post_to .text.esp_event_dump)
    *libesp_event.a:default_event_loop.*(.literal.esp_event_handler_register .literal.esp_event_handler_instance_register .literal.esp_event_handler_unregister .literal.esp_event_handler_instance_unregister .literal.esp_event_post .literal.esp_event_loop_create_default .literal.esp_event_loop_delete_default .literal.esp_event_send_to_default_loop .text.esp_event_handler_register .text.esp_event_handler_instance_register .text.esp_event_handler_unregister .text.esp_event_handler_instance_unregister .text.esp_event_post .text.esp_event_loop_create_default .text.esp_event_loop_delete_default .text.esp_event_send_to_default_loop)
    *libsoc.a:uart_hal_iram.*( .literal  .literal.*  .text  .text.*  .wifi0iram  .wifi0iram.*  .wifirxiram  .wifirxiram.*)

    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.irom0.text) /* catch stray ICACHE_RODATA_ATTR */
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;

    /* Similar to _iram_start, this symbol goes here so it is
       resolved by addr2line in preference to the first symbol in
       the flash.text segment.
    */
    _flash_cache_start = ABSOLUTE(0);
  } >default_code_seg

  /* Marks the end of IRAM code segment */
  .iram0.text_end (NOLOAD) :
  {
    . = ALIGN (4);
    _iram_end = ABSOLUTE(.);
  } > iram0_0_seg

  /* Marks the end of data, bss and possibly rodata  */
  .dram0.heap_start (NOLOAD) :
  {
    . = ALIGN (8);
    _heap_start = ABSOLUTE(.);
  } > dram0_0_seg
}

ASSERT(((_iram_text_end - ORIGIN(iram0_0_seg)) <= LENGTH(iram0_0_seg)),
          "IRAM0 segment data does not fit.")

ASSERT(((_heap_start - ORIGIN(dram0_0_seg)) <= LENGTH(dram0_0_seg)),
          "DRAM segment data does not fit.")
