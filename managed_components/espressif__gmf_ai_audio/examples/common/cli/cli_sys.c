/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_gmf_oal_sys.h"
#include "driver/gpio.h"

#include "argtable3/argtable3.h"

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
#define WITH_TASKS_INFO 1
#endif  /* CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS */

static int restart(int argc, char **argv)
{
    printf("Restarting\n");
    esp_restart();
}

static int free_mem(int argc, char **argv)
{
    printf("\nFree heap size: internal %u, psram %u\nmin  heap size: internal %u, psram %u\n",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));

    return 0;
}

/** 'tasks' command prints the list of tasks and related information */
#if WITH_TASKS_INFO
static int tasks_info(int argc, char **argv)
{
    return esp_gmf_oal_sys_get_real_time_stats(1000, false);
}
#endif  // WITH_TASKS_INFO

static struct {
    struct arg_str *tag;
    struct arg_int *lvl;
    struct arg_end *end;
} log_args;

static char *log_lvl_2_str(int lvl)
{
    switch (lvl) {
        case ESP_LOG_NONE:
            return "NONE";
        case ESP_LOG_ERROR:
            return "ERROR";
        case ESP_LOG_WARN:
            return "WARN";
        case ESP_LOG_INFO:
            return "INFO";
        case ESP_LOG_DEBUG:
            return "DEBUG";
        case ESP_LOG_VERBOSE:
            return "VERBOSE";
        default:
            return "UNKNOWN";
    }
}

static int log_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&log_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, log_args.end, argv[0]);
        return 1;
    }

    if (log_args.tag->count == 0 || log_args.lvl->count == 0) {
        return 2;
    }
    printf("Set log [%s] : %s\n", log_args.tag->sval[0], log_lvl_2_str(log_args.lvl->ival[0]));
    if (log_args.lvl->ival[0] <= ESP_LOG_VERBOSE) {
        esp_log_level_set(log_args.tag->sval[0], log_args.lvl->ival[0]);
    } else {
        return 3;
    }
    return 0;
}

static struct {
    struct arg_int *io;
    struct arg_int *lvl;
    struct arg_end *end;
} io_args;

static int io_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&io_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, io_args.end, argv[0]);
        return 1;
    }

    if (io_args.io->count == 0 || io_args.lvl->count == 0) {
        return 2;
    }
    printf("Set IO [%d] - [%d]\n", io_args.io->ival[0], io_args.lvl->ival[0]);
    if (io_args.lvl->ival[0] == 0 || io_args.lvl->ival[0] == 1) {
        gpio_set_level(io_args.io->ival[0], io_args.lvl->ival[0]);
    } else {
        return 3;
    }
    return 0;
}

void cli_register_sys()
{
    static const esp_console_cmd_t cmds[] = {
        {
            .command = "restart",
            .help = "Software reset of the chip",
            .hint = NULL,
            .func = &restart,
        },
        {
            .command = "free",
            .help = "Get the current size of free heap memory",
            .hint = NULL,
            .func = &free_mem,
        },
#if WITH_TASKS_INFO
        {
            .command = "tasks",
            .help = "Get information about running tasks",
            .hint = NULL,
            .func = &tasks_info,
        },
#endif  /* WITH_TASKS_INFO */
        {
            .command = "log",
            .help = "Set the log mode",
            .hint = NULL,
            .func = &log_set,
            .argtable = &log_args,
        },
        {
            .command = "io",
            .help = "Set the io level",
            .hint = NULL,
            .func = &io_set,
            .argtable = &io_args,
        }};

    log_args.tag = arg_str0(NULL, NULL, "<string>", "TAG of the log want to be set");
    log_args.lvl = arg_int0(NULL, NULL, "<0 - 5>", "Log level want to be set");
    log_args.end = arg_end(2);

    io_args.io = arg_int0(NULL, NULL, "<0 - 40>", "GPIO Number");
    io_args.lvl = arg_int0(NULL, NULL, "<0 - 1>", "IO level want to be set");
    io_args.end = arg_end(2);

    for (int i = 0; i < sizeof(cmds) / sizeof(esp_console_cmd_t); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}
