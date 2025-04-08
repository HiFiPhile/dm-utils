#include <stdlib.h>
#include <string.h>
#include "argtable3.h"
#include "libdm.h"

#define PROGNAME "dm-utils"

#define pr_info(...) fprintf(stdout, __VA_ARGS__)
#define pr_error(...) fprintf(stderr, __VA_ARGS__)

struct app_params {
    struct arg_str *dev;
    struct arg_lit *help;
    struct arg_str *opt;
    struct arg_end *end;
    struct {
        struct arg_rex *cmd;
    } ls;
    struct {
        struct arg_rex *cmd;
    } del;
    struct  {
        struct arg_rex *cmd;
        struct arg_str *cipher;
        struct arg_str *key;
        struct arg_str *block;
    } crypt;
};

static int parse_args(int argc, char **argv, struct app_params *params)
{
    /* Common */
    params->dev             = arg_str1("d", "dev",                  "<device>",                 "Device name");
    params->help            = arg_lit0("h", "help",                                             "Print this help and exit");
    params->opt             = arg_strn("o", "opt",                  "<opt_params>", 0, 99,      "Optional parameter");
    params->end             = arg_end(20);
    /* List */
    params->ls.cmd          = arg_rex1(NULL, NULL,        "ls",     NULL, ARG_REX_ICASE,        "List device");
    /* Delete */
    params->del.cmd         = arg_rex1(NULL, NULL,        "del",    NULL, ARG_REX_ICASE,        "Delete device");
    /* Crypt */
    params->crypt.cmd       = arg_rex1(NULL, NULL,        "crypt",  NULL, ARG_REX_ICASE,        "Create crypt device");
    params->crypt.cipher    = arg_str1("c", "cipher",               "<cipher>",                 "Encryption cipher");
    params->crypt.key       = arg_str1("k", "key",                  "<key>>",                   "Encryption key");
    params->crypt.block     = arg_str1("b", "blockdev",             "<device path>",            "Block device path");

    void* arg_help[]    = {params->help, params->end};
    void* arg_ls[]      = {params->ls.cmd, params->end};
    void* arg_del[]     = {params->del.cmd, params->dev, params->end};
    void* arg_crypt[]   = {params->crypt.cmd, params->dev, params->crypt.cipher, params->crypt.key, params->crypt.block, params->opt, params->end};

    int ret_help = arg_parse(argc, argv, arg_help);
    int ret_ls = arg_parse(argc, argv, arg_ls);
    int ret_del = arg_parse(argc, argv, arg_del);
    int ret_crypt = arg_parse(argc, argv, arg_crypt);

    /* special case: '--help' takes precedence over error reporting */
    if (params->help->count) {
        pr_info("Device-mapper utils.\n\n");
        pr_info("Usage:\n");
        pr_info(PROGNAME);
        arg_print_syntax(stdout, arg_ls, "\n");
        arg_print_glossary(stdout, arg_ls, "  %-30s %s\n");
        pr_info(PROGNAME);
        arg_print_syntax(stdout, arg_del, "\n");
        arg_print_glossary(stdout, arg_del, "  %-30s %s\n");
        pr_info(PROGNAME);
        arg_print_syntax(stdout, arg_crypt, "\n");
        arg_print_glossary(stdout, arg_crypt, "  %-30s %s\n");
        return 0;
    }
    /* If the parser returned any errors then display them and exit */
    if (params->ls.cmd->count) {
        if(ret_ls) {
            arg_print_errors(stderr, params->end, PROGNAME" del");
            pr_error("Try '%s --help' for more information.\n", PROGNAME" del");
            return -1;
        }
    } else if (params->del.cmd->count) {
        if(ret_del) {
            arg_print_errors(stderr, params->end, PROGNAME" del");
            pr_error("Try '%s --help' for more information.\n", PROGNAME" del");
            return -1;
        }
    } else if (params->crypt.cmd->count) {
        if(ret_crypt) {
            arg_print_errors(stderr, params->end, PROGNAME" crypt");
            pr_error("Try '%s --help' for more information.\n", PROGNAME" crypt");
            return -1;
        }
    } else {
        pr_error("Missing <ls|del|crypt> command.\n");
        return -1;
    }

    return 0;
}

static int run_cmds(struct app_params *params)
{
    if (params->ls.cmd->count) {
        return list_blk_dev();
    } else if (params->del.cmd->count) {
        return delete_blk_dev(params->dev->sval[0]);
    } else if(params->crypt.cmd->count) {
        struct crypt_params crypt_params = {
            .name = params->dev->sval[0],
            .cipher = params->crypt.cipher->sval[0],
            .key = params->crypt.key->sval[0],
            .blockdev = params->crypt.block->sval[0],
            .extraparams = "0",
        };

        if (params->opt->count) {
            /* Normally enough*/
            char *optargs = (char*) malloc(128);
            sprintf(optargs, "%d", params->opt->count);

            size_t arglen = strlen(optargs) + 1;
            for(int i = 0; i < params->opt->count; i++) {
                arglen += 1 + strlen(params->opt->sval[i]); /* 1 + for space */
                optargs = (char*) realloc(optargs, arglen);
                strcat(strcat(optargs, " "), params->opt->sval[i]);
            }

            crypt_params.extraparams = optargs;
        }
        return create_crypt_blk_dev(&crypt_params);
    }

    return -1;
}

int main(int argc, char **argv)
{
    struct app_params params;
    memset(&params, 0, sizeof(params));

    if (parse_args(argc, argv, &params) < 0) {
        return -1;
    }

    return run_cmds(&params);
}
