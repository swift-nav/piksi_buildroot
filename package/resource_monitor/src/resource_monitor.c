    /*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <dirent.h>
#include <fnmatch.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <libsbp/linux.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings.h>

#define PROGRAM_NAME "resource_monitor"

#define RESOURCE_USAGE_UPDATE_INTERVAL_MS (1000u)
#define MAX_BYTES_PER_LINE 512
#define MAX_PROCESS_NAME_LENGTH 64
#define MAX_NUM_OF_PROCESS 64*128
#define MAX_OUTPUT_PROCESS 16
#define MAX_VARIABLE_NAME_LENGTH 16
#define MAX_VARIABLE_TYPE_LENGTH 4

#define PS_COMMAND_STR "ps --no-headers -eL -o $'%p\\t%C\\t%z\\t%c\\t%a\\t%u' --sort=-pcpu |head -n 6 >/var/ps-info-output"
#define PS_FILE_NAME "/var/ps-info-output"

typedef struct process_info_s
{
    char process_name[MAX_PROCESS_NAME_LENGTH];
    char user_name[MAX_PROCESS_NAME_LENGTH];
    unsigned int process_id;
    unsigned int meas_count;
    double max_cpu;
    double avg_cpu;
    unsigned int vsz;
    char command[MAX_PROCESS_NAME_LENGTH];
    char argument[MAX_PROCESS_NAME_LENGTH];
}process_info_t;

static struct process_info_s process_info_list[MAX_NUM_OF_PROCESS];


static unsigned int pinfo_count = 0;
static unsigned int one_hz_tick_count = 0;

#define MAX_PARAM_NAME 32

static void dump_resource_statistics_out(void)
{
    unsigned int i = 0;
    fprintf(stderr, "********* CPU usage statistics ***********\n");
    piksi_log(LOG_ERR|LOG_SBP, "********* CPU usage statistics ***********");
    for(; i < pinfo_count; i++)
    {

        fprintf(stderr, "PID %d  user: %s  avg CPU %f  max CPU %f stack : %d count : %d\n", process_info_list[i].process_id,
                            process_info_list[i].user_name,
                            process_info_list[i].avg_cpu,
                            process_info_list[i].max_cpu,
                            process_info_list[i].vsz,
                            process_info_list[i].meas_count);
        piksi_log(LOG_ERR|LOG_SBP, "PID:%d User name %s avg cpu %f max cpu %f  statck : %d count : %d\n", process_info_list[i].process_id,
                  process_info_list[i].user_name,
                  process_info_list[i].avg_cpu,
                  process_info_list[i].max_cpu,
                  process_info_list[i].vsz,
                  process_info_list[i].meas_count);
    }
}

static int get_process_table_idx_by_pid(unsigned int pid)
{
    unsigned int i = 0;
    for(;i<pinfo_count; i++)
    {
        if(process_info_list[i].process_id == pid)
        {
            return (int)i;
        }
    }
    if(pinfo_count >= MAX_NUM_OF_PROCESS-1)
    {
        return -1;
    }
    pinfo_count++;
    process_info_list[i].process_id = pid;
    process_info_list[i].avg_cpu = 0;
    process_info_list[i].max_cpu = 0;
    process_info_list[i].meas_count = 0;
    process_info_list[i].vsz = 0;
    return (int)i;
}

static void update_process_info(int process_idx, process_info_t * new_data)
{
    process_info_list[process_idx].avg_cpu = (process_info_list[process_idx].avg_cpu * process_info_list[process_idx].meas_count + new_data->avg_cpu)/(process_info_list[process_idx].meas_count+1);
    if(new_data->avg_cpu > process_info_list[process_idx].max_cpu)
    {
        process_info_list[process_idx].max_cpu = new_data->avg_cpu;
    }
    process_info_list[process_idx].vsz = new_data->vsz;
    if(process_info_list[process_idx].meas_count == 0)
    {
        strncpy(&(process_info_list[process_idx].command[0]),&(new_data->command[0]),MAX_PROCESS_NAME_LENGTH);
        strncpy(&(process_info_list[process_idx].argument[0]),&(new_data->argument[0]),MAX_PROCESS_NAME_LENGTH);
        strncpy(&(process_info_list[process_idx].user_name[0]),&(new_data->user_name[0]),MAX_PROCESS_NAME_LENGTH);
    }
    process_info_list[process_idx].meas_count++;
}

static void process_top_info_by_line(char * buf)
{
    process_info_t process_data;
    sscanf(buf,"%d\t%lf\t%u\t%s\t%s\t%s",
            &process_data.process_id,
            &process_data.avg_cpu,
            &process_data.vsz,
            &(process_data.command[0]),
            &(process_data.argument[0]),
            &(process_data.user_name[0]));
    int process_idx = get_process_table_idx_by_pid(process_data.process_id);
    update_process_info(process_idx,&process_data);
}

static int update_resource_info()
{
    FILE * fp = NULL;
    system(PS_COMMAND_STR);
    fp = fopen(PS_FILE_NAME,"r");
    if(fp == NULL)
        return -1;
    char buf[MAX_BYTES_PER_LINE];
    while(fgets(&buf[0],MAX_BYTES_PER_LINE, fp))
    {
        process_top_info_by_line(&buf[0]);
    }
    fclose(fp);
    return 0;
}



/**
 * @brief used to trigger usage updates
 */
static void update_proc_metrics(pk_loop_t *loop, void *timer_handle, void *context)
{
    (void)loop;
    (void)timer_handle;
    (void)context;
    update_resource_info();
    if(one_hz_tick_count>=5)
    {
        one_hz_tick_count = 0;
        dump_resource_statistics_out();
    }
    one_hz_tick_count++;
}

static void signal_handler(pk_loop_t *pk_loop, void *handle, void *context)
{
    (void)context;
    int signal_value = pk_loop_get_signal_from_handle(handle);
    piksi_log(LOG_ERR|LOG_SBP, "Caught signal: %d", signal_value);


    pk_loop_stop(pk_loop);
}


static int cleanup(pk_loop_t **pk_loop_loc,
                   int status);

static int parse_options(int argc, char *argv[])
{
    const struct option long_opts[] = {
            {0, 0, 0, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
            default: {
                return 0;
            }
                break;
        }
    }

    return 0;
}


int main(int argc, char *argv[])
{
    pk_loop_t *loop = NULL;
    logging_init(PROGRAM_NAME);
    if (parse_options(argc, argv) != 0) {
        piksi_log(LOG_ERR, "invalid arguments");
        return cleanup(&loop, EXIT_FAILURE);
    }

    loop = pk_loop_create();
    if (loop == NULL) {
        return cleanup(&loop, EXIT_FAILURE);
    }

    if (pk_loop_signal_handler_add(loop, SIGINT, signal_handler, NULL) == NULL) {
        piksi_log(LOG_ERR, "Failed to add SIGINT handler to loop");
    }

    if (pk_loop_timer_add(loop, RESOURCE_USAGE_UPDATE_INTERVAL_MS, update_proc_metrics, NULL) == NULL) {
        return cleanup(&loop, EXIT_FAILURE);
    }

    pk_loop_run_simple(loop);
    piksi_log(LOG_DEBUG, "Resource Daemon: Normal Exit");


    return cleanup(&loop, EXIT_SUCCESS);
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   int status) {
    pk_loop_destroy(pk_loop_loc);
    logging_deinit();
    return status;
}
