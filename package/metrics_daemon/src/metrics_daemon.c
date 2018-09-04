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

#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>

#include <json-c/json.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings.h>

#define PROGRAM_NAME "metrics_daemon"

#define METRICS_ROOT_DIRECTORY "/tmp/metrics"
#define METRICS_OUTPUT_FILENAME "/tmp/messaging_statistics-metrics.json"
#define METRICS_OUTPUT_FULLPATH  METRICS_ROOT_DIRECTORY METRICS_OUTPUT_FILENAME
#define METRICS_USAGE_UPDATE_INTERVAL (10000u)

const char *metrics_path = METRICS_ROOT_DIRECTORY;

#define MAX_FOLDER_NAME_SZ 64
static json_object * jobjRoot;
static json_object * jobjCur;
static char * root_name;
static unsigned int root_length;
static bool first_folder;
static char * target_file = METRICS_OUTPUT_FILENAME;
static char * path = METRICS_ROOT_DIRECTORY;


/**
 * @brief update_json_to_file - function updates json to file
 *
 * @param struct json_object * root: pointer to the root of json object
 * @param const char * file_name: file to write to
 */
static void update_json_to_file(struct json_object * root, const char * file_name)
{
    FILE * fp = fopen(file_name, "a");
    fprintf(fp,"%s\n",json_object_to_json_string(root));
    fclose(fp);
}

/**
 * @brief update_json_to_file - static function that returns file name from path
 *
 * @param const char *str: file path in ASCII
 * @return const char * file_name: file name
 */
static char * extract_filename(const char *str)
{
    int     ch = '/';
    size_t  len;
    const char   *pdest;
    char   *inpfile = NULL;

    // Search backwards for last backslash in filepath
    pdest = strrchr(str, ch);

    // if backslash not found in filepath
    if(pdest == NULL )
    {
        pdest = str;  // The whole name is a file in current path
    }
    else
    {
        pdest++; // Skip the backslash itself.
    }

    // extract filename from file path
    if(pdest[0]=='.')
        pdest++;
    len = strlen(pdest);
    inpfile = calloc(1,MAX_FOLDER_NAME_SZ);  // Make space for the zero.
    strncpy(inpfile, pdest, len+1 ? MAX_FOLDER_NAME_SZ : len+1 < MAX_FOLDER_NAME_SZ);  // Copy including zero.
    return inpfile;
}


/**
 * @brief loop_through_folder_name - function that use the full file path loop through the json object tree to update values
 *
 * @param const char *process_path: file path in ASCII
 * @param const char *root: root name, this is used to pair with the root of the json object tree
 * @param unsigned int root_len: number of chars for the root name
 * @param struct json_object * json_root: root of the json object tree
 * @return struc json_object * : json object node that needs to be updated
 */
static struct json_object *  loop_through_folder_name(const char * process_path, const char * root, unsigned int root_len, struct json_object * json_root)
{
    char     ch = '/';
    const char   *pdest;
    bool found_root = FALSE;
    pdest = strchr(process_path, ch);   ///< get the first slash position
    struct json_object * json_current = json_root;
    while(pdest!=NULL)
    {
        pdest++;
        if(pdest[0]=='.')   ///< skip the '.' on the file name
            pdest++;
        char * pdest2 = strchr(pdest, ch);   ///< search the second slash
        if(pdest2!=NULL)
        {
            unsigned int strlen;
            if(pdest2<=pdest)               ///< defensive code
                return json_current;
            else
                strlen = (unsigned)(pdest2-pdest);    ///< cast to unsigned since the negative and zero case is handled by the if statement
            if(!found_root &&strncmp(pdest,root,root_len)==0)   ///< search the root first
            {
                found_root = TRUE;
                pdest--;
                continue;
            }
            if(found_root)
            {
                bool found_target = FALSE;
                json_object_object_foreach(json_current, key, val) {        ///< for each subfolder, search the name as key in json tree
                    char tempStr[80];
                    memcpy(&tempStr[0], (pdest), strlen);
                    if(strncmp((pdest),key,strlen-1)==0)        ///< if find the folder name, continue on the loop with the new current node
                    {
                        json_current = val;
                        found_target = TRUE;
                        break;
                    }
                }
                if(!found_target)               ///< if couldn't find target on the tree, return current node to add new node
                    return json_current;
            }
        }
        pdest = pdest2; ///< if there is still sub-folders, update the pdest to pdest2
    }
    return json_current;
}


/**
 * @brief handle_walk_path - function get called by ftw for each of the sub-folder or file under the path
 *
 * @param const char *fpath: file path in ASCII
 * @param const struct stat *sb: file status struct
 * @param int tflag: flag indicates file/folder type
 * @return int : integer that indicates if the walk through failed.
 */
static int handle_walk_path(const char *fpath, const struct stat *sb, int tflag)
{
    if(first_folder == TRUE)     ///< skip the root since it is handled by main()
    {
        first_folder = FALSE;
        return 0;
    }
    char * bname = extract_filename(fpath);
    json_object * jobj_Cur = loop_through_folder_name(fpath,root_name, root_length, jobjRoot);  ///< get the json object needs to be updated
    switch(tflag)
    {
        case FTW_D:
        {
            bool file_entry_exist = FALSE;
            json_object_object_foreach(jobj_Cur, key, val) {
                if(strcmp(bname,key)==0)
                {
                    file_entry_exist = TRUE;
                    jobjCur = val;
                    break;
                }
            }
            if(!file_entry_exist)                 ///< if it is a folder and it doesn't exist in json tree, add a new folder, do nothing otherwise
            {
                json_object *jnewobj = json_object_new_object();     ///< Create a new object here to set the name of folder
                json_object_object_add(jobj_Cur,bname, jnewobj);
            }
            break;
        }

        case FTW_F:
        {
            FILE * fp = fopen(fpath, "r");           ///< read file to get the data
            char buf[64];
            long file_len_long = sb->st_size-1;
            int file_len = (int)(sb->st_size-1);
            if(file_len<64 && file_len_long >=0)
            {
                fread(&buf[0],(unsigned int)file_len,1,fp);
            } else{
                file_len = 0;                        ///< if file is empty, set the str length to zero
            }
            fclose(fp);
            bool file_entry_exist = FALSE;
            json_object_object_foreach(jobj_Cur, key, val) {
                if(strcmp(bname,key)==0)
                {
                    file_entry_exist = TRUE;
                    json_object_set_string_len(val,&buf[0],file_len);     ///< if file exist, update
                    break;
                }
            }
            if(!file_entry_exist)     ///< add a new node if it doesn't exist
            {
                json_object_object_add(jobj_Cur,bname, json_object_new_string_len(&buf[0],file_len));
            }
            break;
        }
        default:
        {
            perror("FTW: unexpected file type\n");   ///< For now, only support file and forlder
            return -1;
        }
    }

    free(bname);  ///< free the file name allocated under loop_through_folder_name()
    return 0;
}
/**
 * @brief write_messaging_metrics_to_file
 *
 * Write messaging metrics to file
 * @param metrics_dir: directory that holds the metrics directory tree structure
 */
static void write_messaging_metrics_to_file(const char *metrics_dir) {
    (void) metrics_dir;
    /* Logic to write metrics info here */
    // Walk dir for metrics
    first_folder = TRUE;
    if (ftw(metrics_dir, handle_walk_path, 20) == -1) {
        return;
    }
    update_json_to_file(jobjRoot, target_file);
}


static void usage(char *command)
{
    printf("Usage: %s\n", command);

    puts("\nMain options");
}

static int parse_options(int argc, char *argv[])
{
    enum {
        OPT_ID_METRICS_PATH = 1
    };

    const struct option long_opts[] = {
            {"metrics_path", required_argument, 0, OPT_ID_METRICS_PATH},
            {0, 0, 0, 0},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
            case OPT_ID_METRICS_PATH: {
                path = optarg;
            }
                break;

            default: {
                puts("Invalid option");
                return -1;
            }
                break;
        }
    }

    return 0;
}

/**
 * @brief usage_timer_callback - used to trigger usage updates
 */
static void usage_timer_callback(pk_loop_t *loop, void *timer_handle, void *context)
{
    (void)loop;
    (void)timer_handle;
    (void)context;

    write_messaging_metrics_to_file(metrics_path);
}

static void signal_handler(pk_loop_t *pk_loop, void *handle, void *context)
{
    (void)context;
    int signal_value = pk_loop_get_signal_from_handle(handle);

    piksi_log(LOG_DEBUG, "Caught signal: %d", signal_value);

    pk_loop_stop(pk_loop);
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   int status);

int main(int argc, char *argv[])
{
    pk_loop_t *loop = NULL;

    logging_init(PROGRAM_NAME);

    if (parse_options(argc, argv) != 0) {
        piksi_log(LOG_ERR, "invalid arguments");
        usage(argv[0]);
        return cleanup(&loop, EXIT_FAILURE);
    }
    jobjRoot = json_object_new_object();
    jobjCur = json_object_new_object();
    char * bname = extract_filename(path);   ///< for the file name, clearn it up in cleanup
    root_length = sizeof(root_name);
    root_name = bname;
    json_object_object_add(jobjRoot,bname, jobjCur);
    loop = pk_loop_create();
    if (loop == NULL) {
        return cleanup(&loop, EXIT_FAILURE);
    }

    if (pk_loop_signal_handler_add(loop, SIGINT, signal_handler, NULL) == NULL) {
        piksi_log(LOG_ERR, "Failed to add SIGINT handler to loop");
    }

    if (pk_loop_timer_add(loop, METRICS_USAGE_UPDATE_INTERVAL, usage_timer_callback, NULL) == NULL) {
        return cleanup(&loop, EXIT_FAILURE);
    }

    pk_loop_run_simple(loop);
    piksi_log(LOG_DEBUG, "Metrics Daemon: Normal Exit");


    return cleanup(&loop, EXIT_SUCCESS);
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   int status) {
    pk_loop_destroy(pk_loop_loc);
    free(root_name);
    logging_deinit();
    json_object_put(jobjRoot);
    return status;
}
