#include <stdlib.h>
#include <string.h>
#include "argtable3.h"
#include "libdm.h"

#define PROGNAME "dm-utils"

int main(int argc, char **argv)
{
    /* Common */
    struct arg_str *dev     = arg_str1("d", "dev",                  "<device>",                 "Device name");
    struct arg_lit *help    = arg_lit0("h", "help",                                             "Print this help and exit");
    /* List */
    struct arg_rex *ls      = arg_rex1(NULL, NULL,        "ls",     NULL, ARG_REX_ICASE,        "List device");
    struct arg_end *end0    = arg_end(20);
    /* Delete */
    struct arg_rex *del     = arg_rex1(NULL, NULL,        "del",    NULL, ARG_REX_ICASE,        "Delete device");
    struct arg_end *end1    = arg_end(20);
    /* Crypt */
    struct arg_rex *crypt   = arg_rex1(NULL, NULL,        "crypt",  NULL, ARG_REX_ICASE,        "Create crypt device");
    struct arg_str *cipher  = arg_str1("c", "cipher",               "<cipher>",                 "Encryption cipher");
    struct arg_str *key     = arg_str1("k", "key",                  "<key>>",                   "Encryption key");
    struct arg_str *block   = arg_str1("b", "blockdev",             "<device path>",            "Block device path");
    struct arg_str *opt     = arg_strn("o", "opt",                  "<opt_params>", 0, 99,      "Optional parameter");
    struct arg_end *end2    = arg_end(20);

    void* arg_ls[]      = {ls, help, end1};
    void* arg_del[]     = {del, dev, help, end1};
    void* arg_crypt[]   = {crypt, dev, cipher, key, block, opt, help, end2};

    int nerrors0 = arg_parse(argc, argv, arg_ls);
    int nerrors1 = arg_parse(argc, argv, arg_del);
    int nerrors2 = arg_parse(argc, argv, arg_crypt);

    /* special case: '--help' takes precedence over error reporting */
    if (help->count) {
        printf("Device-mapper utils.\n\n");
        if (ls->count) {
            arg_print_syntax(stdout, arg_ls, "\n");
            arg_print_glossary(stdout, arg_ls, "  %-30s %s\n");
        } else if (del->count) {
            arg_print_syntax(stdout, arg_del, "\n");
            arg_print_glossary(stdout, arg_del, "  %-30s %s\n");
        } else if (crypt->count) {
            arg_print_syntax(stdout, arg_crypt, "\n");
            arg_print_glossary(stdout, arg_crypt, "  %-30s %s\n");
        } else {
            printf("Missing <ls|del|crypt> command.\n");
        }
        return 0;
    }
    /* If the parser returned any errors then display them and exit */
    if (ls->count) {
        if(nerrors0) {
            arg_print_errors(stdout, end1, PROGNAME" del");
            printf("Try '%s --help' for more information.\n", PROGNAME" del");
            exit(-1);
        }
    } else if (del->count) {
        if(nerrors1) {
            arg_print_errors(stdout, end1, PROGNAME" del");
            printf("Try '%s --help' for more information.\n", PROGNAME" del");
            exit(-1);
        }
    } else if (crypt->count) {
        if(nerrors2) {
            arg_print_errors(stdout, end2, PROGNAME" crypt");
            printf("Try '%s --help' for more information.\n", PROGNAME" crypt");
            exit(-1);
        }
    } else {
        printf("Missing <ls|del|crypt> command.\n");
        exit(-1);
    }

    if (ls->count) {
        return list_blk_dev();
    } else if (del->count) {
        return delete_blk_dev(dev->sval[0]);
    } else if(crypt->count) {
        struct crypt_params params = {
            .name = dev->sval[0],
            .cipher = cipher->sval[0],
            .key = key->sval[0],
            .blockdev = block->sval[0],
            .extraparams = "0",
        };

        if (opt->count) {
            /* Normally enough*/
            char *optargs = (char*) malloc(128);
            sprintf(optargs, "%d", opt->count);

            size_t arglen = strlen(optargs) + 1;
            for(int i = 0; i < opt->count; i++) {
                arglen += 1 + strlen(opt->sval[i]); /* 1 + for space */
                optargs = (char*) realloc(optargs, arglen);
                strcat(strcat(optargs, " "), opt->sval[i]);
            }

            params.extraparams = optargs;
        }
        return create_crypt_blk_dev(&params);
    }

    return 0;
}