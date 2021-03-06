// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <pegasus/version.h>
#include <dsn/utility/strings.h>
#include <setjmp.h>
#include <signal.h>
#include <algorithm>
#include "args.h"
#include "command_executor.h"
#include "commands.h"

std::string g_last_history;
const int s_max_params_count = 10000;
std::map<std::string, command_executor *> s_commands_map;
shell_context s_global_context;
size_t s_max_name_length = 0;
size_t s_option_width = 70;

void print_help();
bool help_info(command_executor *e, shell_context *sc, arguments args)
{
    print_help();
    return true;
}

command_executor commands[] = {
    {
        "help", "print help info", "", help_info,
    },
    {
        "version", "get the shell version", "", version,
    },
    {
        "cluster_info", "get the informations for the cluster", "", query_cluster_info,
    },
    {
        "app",
        "get the partition information for some specific app",
        "<app_name> [-d|--detailed] [-o|--output file_name]",
        query_app,
    },
    {
        "app_disk",
        "get the disk usage information for some specific app",
        "<app_name> [-d|--detailed] [-o|--output file_name]",
        app_disk,
    },
    {
        "ls",
        "list all apps",
        "[-a|-all] [-d|--detailed] [-o|--output file_name] "
        "[-s|--status all|available|creating|dropping|dropped]",
        ls_apps,
    },
    {
        "nodes",
        "get the node status for this cluster",
        "[-d|--detailed] [-o|--output file_name] [-s|--status all|alive|unalive]",
        ls_nodes,
    },
    {
        "create",
        "create an app",
        "<app_name> [-p|--partition_count num] [-r|--replica_count num] "
        "[-e|--envs k1=v1,k2=v2...]",
        create_app,
    },
    {
        "drop", "drop an app", "<app_name> [-r|--reserve_seconds num]", drop_app,
    },
    {
        "recall", "recall an app", "<app_id> [new_app_name]", recall_app,
    },
    {
        "set_meta_level",
        "set the meta function level: stopped, blind, freezed, steady, lively",
        "<stopped|blind|freezed|steady|lively>",
        set_meta_level,
    },
    {
        "get_meta_level", "get the meta function level", "", get_meta_level,
    },
    {
        "balance",
        "send explicit balancer request for the cluster",
        "<-g|--gpid appid.pidx> <-p|--type move_pri|copy_pri|copy_sec> <-f|--from from_address> "
        "<-t|--to to_address>",
        balance,
    },
    {
        "propose",
        "send configuration proposals to cluster",
        "[-f|--force] "
        "<-g|--gpid appid.pidx> <-p|--type ASSIGN_PRIMARY|ADD_SECONDARY|DOWNGRADE_TO_INACTIVE...> "
        "<-t|--target node_to_exec_command> <-n|--node node_to_be_affected> ",
        propose,
    },
    {
        "use",
        "set the current app used for the data access commands",
        "[app_name]",
        use_app_as_current,
    },
    {
        "escape_all",
        "if escape all characters when printing key/value bytes",
        "[true|false]",
        process_escape_all,
    },
    {
        "timeout",
        "default timeout in milliseconds for read/write operations",
        "[time_in_ms]",
        process_timeout,
    },
    {
        "hash",
        "calculate the hash result for some hash key",
        "<hash_key> <sort_key>",
        calculate_hash_value,
    },
    {
        "set", "set value", "<hash_key> <sort_key> <value> [ttl_in_seconds]", data_operations,
    },
    {
        "multi_set",
        "set multiple values for a single hash key",
        "<hash_key> <sort_key> <value> [sort_key value...]",
        data_operations,
    },
    {
        "get", "get value", "<hash_key> <sort_key>", data_operations,
    },
    {
        "multi_get",
        "get multiple values for a single hash key",
        "<hash_key> [sort_key...]",
        data_operations,
    },
    {
        "multi_get_range",
        "get multiple values under sort key range for a single hash key",
        "<hash_key> <start_sort_key> <stop_sort_key> "
        "[-a|--start_inclusive true|false] [-b|--stop_inclusive true|false] "
        "[-s|--sort_key_filter_type anywhere|prefix|postfix] "
        "[-y|--sort_key_filter_pattern str] "
        "[-n|--max_count num] [-i|--no_value] [-r|--reverse]",
        data_operations,
    },
    {
        "multi_get_sortkeys",
        "get multiple sort keys for a single hash key",
        "<hash_key>",
        data_operations,
    },
    {
        "del", "delete a key", "<hash_key> <sort_key>", data_operations,
    },
    {
        "multi_del",
        "delete multiple values for a single hash key",
        "<hash_key> <sort_key> [sort_key...]",
        data_operations,
    },
    {
        "multi_del_range",
        "delete multiple values under sort key range for a single hash key",
        "<hash_key> <start_sort_key> <stop_sort_key> "
        "[-a|--start_inclusive true|false] [-b|--stop_inclusive true|false] "
        "[-s|--sort_key_filter_type anywhere|prefix|postfix] "
        "[-y|--sort_key_filter_pattern str] "
        "[-o|--output file_name] [-i|--silent]",
        data_operations,
    },
    {
        "exist", "check value exist", "<hash_key> <sort_key>", data_operations,
    },
    {
        "count", "get sort key count for a single hash key", "<hash_key>", data_operations,
    },
    {
        "ttl", "query ttl for a specific key", "<hash_key> <sort_key>", data_operations,
    },
    {
        "hash_scan",
        "scan all sorted keys for a single hash key",
        "<hash_key> <start_sort_key> <stop_sort_key> "
        "[-a|--start_inclusive true|false] [-b|--stop_inclusive true|false] "
        "[-s|--sort_key_filter_type anywhere|prefix|postfix] "
        "[-y|--sort_key_filter_pattern str] "
        "[-o|--output file_name] [-n|--max_count num] [-t|--timeout_ms num] "
        "[-d|--detailed] [-i|--no_value]",
        data_operations,
    },
    {
        "full_scan",
        "scan all hash keys",
        "[-h|--hash_key_filter_type anywhere|prefix|postfix] "
        "[-x|--hash_key_filter_pattern str] "
        "[-s|--sort_key_filter_type anywhere|prefix|postfix] "
        "[-y|--sort_key_filter_pattern str] "
        "[-o|--output file_name] [-n|--max_count num] [-t|--timeout_ms num] "
        "[-d|--detailed] [-i|--no_value] [-p|--partition num]",
        data_operations,
    },
    {
        "copy_data",
        "copy app data",
        "<-c|--target_cluster_name str> <-a|--target_app_name str> "
        "[-s|--max_split_count num] [-b|--max_batch_count num] [-t|--timeout_ms num]",
        data_operations,
    },
    {
        "clear_data",
        "clear app data",
        "[-f|--force] [-s|--max_split_count num] [-b|--max_batch_count num] "
        "[-t|--timeout_ms num]",
        data_operations,
    },
    {
        "count_data",
        "get app row count",
        "[-s|--max_split_count num] [-b|--max_batch_count num] "
        "[-t|--timeout_ms num] [-z|--stat_size] [-c|--top_count num]",
        data_operations,
    },
    {
        "remote_command",
        "send remote command to servers",
        "[-t all|meta-server|replica-server] [-l ip:port,ip:port...] "
        "<command> [arguments...]",
        remote_command,
    },
    {
        "server_info",
        "get info of servers",
        "[-t all|meta-server|replica-server] [-l ip:port,ip:port...]",
        server_info,
    },
    {
        "server_stat",
        "get stat of servers",
        "[-t all|meta-server|replica-server] [-l ip:port,ip:port...]",
        server_stat,
    },
    {
        "app_stat", "get stat of apps", "[-a|--app_name str] [-o|--output file_name]", app_stat,
    },
    {
        "flush_log",
        "flush log of servers",
        "[-t all|meta-server|replica-server] [-l ip:port,ip:port...]",
        flush_log,
    },
    {
        "local_get", "get value from local db", "<db_path> <hash_key> <sort_key>", local_get,
    },
    {
        "sst_dump",
        "dump sstable dir or files",
        "[--command=check|scan|none|raw] <--file=data_dir_OR_sst_file> "
        "[--from=user_key] [--to=user_key] [--read_num=num] [--show_properties]",
        sst_dump,
    },
    {
        "mlog_dump",
        "dump mutation log dir",
        "<-i|--input log_dir> [-o|--output file_name] [-d|--detailed]",
        mlog_dump,
    },
    {
        "recover",
        "control the meta to recover the system from given nodes",
        "[-f|--node_list_file file_name] [-s|--node_list_str str] "
        "[-w|--wait_seconds num] "
        "[-b|--skip_bad_nodes] [-l|--skip_lost_partitions] [-o|--output file_name]",
        recover,
    },
    {
        "add_backup_policy",
        "add new cold backup policy",
        "<-p|--policy_name str> <-b|--backup_provider_type str> <-a|--app_ids 1,2...> "
        "<-i|--backup_interval_seconds num> <-s|--start_time hour:minute> "
        "<-c|--backup_history_cnt num>",
        add_backup_policy,
    },
    {"ls_backup_policy", "list the names of the subsistent backup policies", "", ls_backup_policy},
    {
        "query_backup_policy",
        "query subsistent backup policy and last backup infos",
        "<-p|--policy_name p1,p2...> [-b|--backup_info_cnt num]",
        query_backup_policy,
    },
    {
        "modify_backup_policy",
        "modify the backup policy",
        "<-p|--policy_name str> [-a|--add_app 1,2...] [-r|--remove_app 1,2...] "
        "[-i|--backup_interval_seconds num] [-c|--backup_history_count num] "
        "[-s|--start_time hour:minute]",
        modify_backup_policy,
    },
    {
        "disable_backup_policy",
        "stop policy continue backup",
        "<-p|--policy_name str>",
        disable_backup_policy,
    },
    {
        "enable_backup_policy",
        "start backup policy to backup again",
        "<-p|--policy_name str>",
        enable_backup_policy,
    },
    {
        "restore_app",
        "restore app from backup media",
        "<-c|--old_cluster_name str> <-p|--old_policy_name str> <-a|--old_app_name str> "
        "<-i|--old_app_id id> <-t|--timestamp/backup_id timestamp> "
        "<-b|--backup_provider_type str> [-n|--new_app_name str] [-s|--skip_bad_partition]",
        restore,
    },
    {
        "query_restore_status",
        "query restore status",
        "<restore_app_id> [-d|--detailed]",
        query_restore_status,
    },
    {
        "get_app_envs", "get current app envs", "", get_app_envs,
    },
    {
        "set_app_envs", "set current app envs", "<key> <value> [key value...]", set_app_envs,
    },
    {
        "del_app_envs", "delete current app envs", "<key> [key...]", del_app_envs,
    },
    {
        "clear_app_envs", "clear current app envs", "<-a|--all> <-p|--prefix str>", clear_app_envs,
    },
    {
        "exit", "exit shell", "", exit_shell,
    },
    {
        nullptr, nullptr, nullptr, nullptr,
    }};

void print_help(command_executor *e, size_t name_width, size_t option_width)
{
    std::vector<std::string> lines;
    std::string options(e->option_usage);
    int line_start = 0;
    int line_end = -1;
    int i;
    for (i = 0; i < options.size(); i++) {
        if (i - line_start >= option_width && line_end >= line_start) {
            std::string s = options.substr(line_start, line_end - line_start + 1);
            std::string r = dsn::utils::trim_string((char *)s.c_str());
            if (!r.empty())
                lines.push_back(r);
            line_start = line_end + 2;
        }
        if ((options[i] == ']' || options[i] == '>') && i < options.size() - 1 &&
            options[i + 1] == ' ') {
            line_end = i;
        }
    }
    line_end = i - 1;
    if (line_end >= line_start) {
        std::string s = options.substr(line_start, line_end - line_start + 1);
        std::string r = dsn::utils::trim_string((char *)s.c_str());
        if (!r.empty())
            lines.push_back(r);
    }

    std::cout << "\t" << e->name << std::string(name_width + 2 - strlen(e->name), ' ');
    if (lines.empty()) {
        std::cout << std::endl;
    } else {
        for (int k = 0; k < lines.size(); k++) {
            if (k != 0)
                std::cout << "\t" << std::string(name_width + 2, ' ');
            std::cout << lines[k] << std::endl;
        }
    }
}

void print_help()
{
    std::cout << "Usage:" << std::endl;
    for (int i = 0; commands[i].name != nullptr; ++i) {
        print_help(&commands[i], s_max_name_length, s_option_width);
    }
}

void register_all_commands()
{
    for (int i = 0; commands[i].name != nullptr; ++i) {
        auto pr = s_commands_map.emplace(commands[i].name, &commands[i]);
        dassert(pr.second, "the command '%s' is already registered!!!", commands[i].name);
        s_max_name_length = std::max(s_max_name_length, strlen(commands[i].name));
    }
}

void execute_command(command_executor *e, int argc, std::string str_args[])
{
    static char buffer[s_max_params_count][512]; // 512*32
    static char *argv[s_max_params_count];
    for (int i = 0; i < s_max_params_count; ++i) {
        argv[i] = buffer[i];
    }

    for (int i = 0; i < argc && i < s_max_params_count; ++i) {
        if (!str_args[i].empty()) {
            strcpy(argv[i], str_args[i].c_str());
        } else {
            memset(argv[i], 0, sizeof(512));
        }
    }

    if (!e->exec(e, &s_global_context, {argc, argv})) {
        printf("USAGE: ");
        print_help(e, s_max_name_length, s_option_width);
    }
}

static sigjmp_buf s_ctrlc_buf;
void handle_signals(int signo)
{
    if (signo == SIGINT) {
        std::cout << std::endl << "Type \"Ctrl-D\" to exit the shell." << std::endl;
        siglongjmp(s_ctrlc_buf, 1);
    }
}

void initialize(int argc, char **argv)
{
    if (signal(SIGINT, handle_signals) == SIG_ERR) {
        std::cout << "ERROR: register signal handler failed" << std::endl;
        dsn_exit(-1);
    }

    std::cout << "Pegasus Shell " << PEGASUS_VERSION << std::endl;
    std::cout << "Type \"help\" for more information." << std::endl;
    std::cout << "Type \"Ctrl-D\" to exit the shell." << std::endl;
    std::cout << std::endl;

    std::string config_file = argc > 1 ? argv[1] : "config.ini";
    if (!pegasus::pegasus_client_factory::initialize(config_file.c_str())) {
        std::cout << "ERROR: init pegasus failed: " << config_file << std::endl;
        dsn_exit(-1);
    } else {
        std::cout << "The config file is: " << config_file << std::endl;
    }

    std::string cluster_name = argc > 2 ? argv[2] : "mycluster";
    std::cout << "The cluster name is: " << cluster_name << std::endl;

    s_global_context.current_cluster_name = cluster_name;
    std::string section = "uri-resolver.dsn://" + s_global_context.current_cluster_name;
    std::string key = "arguments";
    std::string server_list = dsn_config_get_value_string(section.c_str(), key.c_str(), "", "");
    std::cout << "The cluster meta list is: " << server_list << std::endl;

    dsn::replication::replica_helper::load_meta_servers(
        s_global_context.meta_list, section.c_str(), key.c_str());
    s_global_context.ddl_client =
        new dsn::replication::replication_ddl_client(s_global_context.meta_list);

    register_all_commands();
}

void run()
{
    while (sigsetjmp(s_ctrlc_buf, 1) != 0)
        ;

    while (true) {
        int arg_count;
        std::string args[s_max_params_count];
        scanfCommand(arg_count, args, s_max_params_count);
        if (arg_count > 0) {
            int i = 0;
            for (; i < arg_count; ++i) {
                std::string &s = args[i];
                int j = 0;
                for (; j < s.size(); ++j) {
                    if (!isprint(s.at(j))) {
                        std::cout << "ERROR: found unprintable character in '"
                                  << pegasus::utils::c_escape_string(s) << "'" << std::endl;
                        break;
                    }
                }
                if (j < s.size())
                    break;
            }
            if (i < arg_count)
                continue;
            auto iter = s_commands_map.find(args[0]);
            if (iter != s_commands_map.end()) {
                execute_command(iter->second, arg_count, args);
            } else {
                std::cout << "ERROR: invalid subcommand '" << args[0] << "'" << std::endl;
                print_help();
            }
        }
    }
}

int main(int argc, char **argv)
{
    initialize(argc, argv);
    run();
    return 0;
}

#if defined(__linux__)
#include <dsn/git_commit.h>
#include <dsn/version.h>
#include <pegasus/git_commit.h>
#include <pegasus/version.h>
static char const rcsid[] =
    "$Version: Pegasus Shell " PEGASUS_VERSION " (" PEGASUS_GIT_COMMIT ")"
#if defined(DSN_BUILD_TYPE)
    " " STR(DSN_BUILD_TYPE)
#endif
        ", built with rDSN " DSN_CORE_VERSION " (" DSN_GIT_COMMIT ")"
        ", built by gcc " STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__)
#if defined(DSN_BUILD_HOSTNAME)
            ", built on " STR(DSN_BUILD_HOSTNAME)
#endif
                ", built at " __DATE__ " " __TIME__ " $";
const char *pegasus_shell_rcsid() { return rcsid; }
#endif
