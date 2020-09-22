board_init_f 完成串口和 ddr 的初始化，以及做一部分重定位的前期工作，保留空间。  

crt0.S 中完成重定位的工作，包括代码和 vector 的重定位，以及 rel_dynmic 的重定位(？)

board_init_r 定义在 common/board_r.c 中，完成一部分硬件的初始化。 

init_sequence_r：

    initr_trace                               
                                            //本文件中定义，如果定义了 CONFIG_TRACE，就初始化trace系统
    initr_reloc                            //本文件中定义，设置 gd->flags,表示重定位完成
    initr_caches
    initr_reloc_global_data,               //初始化一些变量
    initr_barrier,                         //执行 asm("sync ; isync");
    initr_malloc,                          初始化堆空间
    initr_console_record,                  没有定义 CONFIG_CONSOLE_RECORD，不做任何事
    bootstage_relocate,                    
    initr_dm,                              设备管理初始化
    initr_bootstage,                       指定当前 bootstage 为 board_init_r
    board_init,                            
                                            定义在 board/freescale/mx6ullevk/mx6ullevk.c 中，主要是完成硬件的初始化，I2c。、qspi、nand
    efi_memory_init,                        
    stdio_init_tables, 
    initr_serial,
    initr_announce,
    INIT_FUNC_WATCHDOG_RESET
    INIT_FUNC_WATCHDOG_RESET
    INIT_FUNC_WATCHDOG_RESET
    power_init_board,
    INIT_FUNC_WATCHDOG_RESET
    initr_mmc,
    initr_env,
    INIT_FUNC_WATCHDOG_RESET
    initr_secondary_cpu,
    INIT_FUNC_WATCHDOG_RESET
    stdio_add_devices,
    initr_jumptable,
    console_init_r,
    INIT_FUNC_WATCHDOG_RESET
    interrupt_init,
    initr_enable_interrupts,
    initr_ethaddr,
    board_late_init,
    INIT_FUNC_WATCHDOG_RESET
    initr_net,



最终的阶段，进入命令行界面，找到命令行的解析器：
    run_main_loop,





定义：
CONFIG_FEC_MXC
CONFIG_FSL_QSPI


pinmux 配置基地址：0x44E1_0000

setup_fec(CONFIG_FEC_ENET_DEV);  只是使能 fec 的 clock
board_qspi_init();
