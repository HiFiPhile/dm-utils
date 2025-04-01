/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2025 HiFiPhile
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/dm-ioctl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "libdm.h"

#define pr_info(...) fprintf(stdout, __VA_ARGS__)
#define pr_error(...) fprintf(stderr, __VA_ARGS__)

#define DM_CRYPT_BUF_SIZE 4096
#define TABLE_LOAD_RETRIES 10
#define DEVMAPPER_CONTROL_FILE "/dev/mapper/control"

static int mkpath(const char* file_path, mode_t mode) {
    if (!file_path || !*file_path)
        return -1;
    for (char* p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

static unsigned long get_blkdev_size(int fd)
{
    unsigned long nr_sec;

    if ( (ioctl(fd, BLKGETSIZE, &nr_sec)) == -1) {
        nr_sec = 0;
    }

    return nr_sec;
}

static void ioctl_init(struct dm_ioctl *io, size_t dataSize, const char *name, unsigned flags)
{
    memset(io, 0, dataSize);
    io->data_size = dataSize;
    io->data_start = sizeof(struct dm_ioctl);
    io->version[0] = 4;
    io->version[1] = 0;
    io->version[2] = 0;
    io->flags = flags;
    if (name) {
        strncpy(io->name, name, sizeof(io->name));
    }
}

static int load_crypto_mapping_table(struct crypt_params *params,  int fd, unsigned long blksz)
{
    char buffer[DM_CRYPT_BUF_SIZE];
    struct dm_ioctl *io;
    struct dm_target_spec *tgt;
    char *crypt_params;
    int i;

    io = (struct dm_ioctl *) buffer;

    /* Load the mapping table for this device */
    tgt = (struct dm_target_spec *) &buffer[sizeof(struct dm_ioctl)];

    ioctl_init(io, DM_CRYPT_BUF_SIZE, params->name, 0);
    io->target_count = 1;
    tgt->status = 0;
    tgt->sector_start = 0;
    tgt->length = blksz;
    crypt_params = buffer + sizeof(struct dm_ioctl) + sizeof(struct dm_target_spec);

    strlcpy(tgt->target_type, "crypt", DM_MAX_TYPE_NAME);

    snprintf(crypt_params, sizeof(buffer) - sizeof(struct dm_ioctl) - sizeof(struct dm_target_spec) - 8,
         "%s %s 0 %s 0 %s", params->cipher, params->key, params->blockdev, params->extraparams);

    crypt_params += strlen(crypt_params) + 1;
    crypt_params = (char *) (((unsigned long)crypt_params + 7) & ~8); /* Align to an 8 byte boundary */
    tgt->next = crypt_params - buffer;

    for (i = 0; i < TABLE_LOAD_RETRIES; i++) {
        if (!ioctl(fd, DM_TABLE_LOAD, io)) {
            break;
        }
        usleep(500000);
    }

    if (i == TABLE_LOAD_RETRIES) {
        /* We failed to load the table, return an error */
        return -1;
    } else {
        return i + 1;
    }
}

int create_crypt_blk_dev(struct crypt_params *params)
{
    char buffer[DM_CRYPT_BUF_SIZE];
    int retval = -1;

    int fd = open(params->blockdev, O_RDONLY|O_CLOEXEC);
    if (fd == -1) {
        pr_error("Failed to open %s: %s", params->blockdev, strerror(errno));
        return -1;
    }

    unsigned long nr_sec = 0;
    nr_sec = get_blkdev_size(fd);
    close(fd);

    if (nr_sec == 0) {
        pr_error("Failed to get size of %s: %s", params->blockdev, strerror(errno));
        return -1;
    }

    if ((fd = open(DEVMAPPER_CONTROL_FILE, O_RDWR)) < 0 ) {
        pr_error("Cannot open device-mapper\n");
        goto out;
    }

    struct dm_ioctl *io = (struct dm_ioctl *) buffer;

    ioctl_init(io, DM_CRYPT_BUF_SIZE, params->name, 0);
    if (ioctl(fd, DM_DEV_CREATE, io)) {
        pr_error("Cannot create dm-crypt device %i\n", errno);
        goto out;
    }

    /* Get the device status, in particular, the name of it's device file */
    ioctl_init(io, DM_CRYPT_BUF_SIZE, params->name, 0);
    if (ioctl(fd, DM_DEV_STATUS, io)) {
        pr_error("Cannot retrieve dm-crypt device status\n");
        goto remove;
    }

    int load_count = load_crypto_mapping_table(params, fd, nr_sec);
    if (load_count < 0) {
        pr_error("Cannot load dm-crypt mapping table.\n");
        goto remove;
    } else if (load_count > 1) {
        pr_error("Took %d tries to load dmcrypt table.\n", load_count);
    }

    /* Resume this device to activate it */
    ioctl_init(io, DM_CRYPT_BUF_SIZE, params->name, 0);

    if (ioctl(fd, DM_DEV_SUSPEND, io)) {
        pr_error("Cannot resume the dm-crypt device\n");
        goto remove;
    }

    /* We made it here with no errors.  Woot! */
    retval = 0;
    goto out;

remove:
    ioctl_init(io, DM_CRYPT_BUF_SIZE, params->name, 0);
    ioctl(fd, DM_DEV_REMOVE, io);

out:
    close(fd);

    return retval;
}

int delete_blk_dev(const char *name)
{
    int fd;
    char buffer[DM_CRYPT_BUF_SIZE];
    struct dm_ioctl *io;
    int retval = -1;

    if ((fd = open(DEVMAPPER_CONTROL_FILE, O_RDWR)) < 0 ) {
        pr_error("Cannot open device-mapper\n");
        goto errout;
    }

    io = (struct dm_ioctl *) buffer;

    ioctl_init(io, DM_CRYPT_BUF_SIZE, name, 0);
    if (ioctl(fd, DM_DEV_REMOVE, io)) {
        pr_error("Cannot remove dm-crypt device %s\n", name);
        goto errout;
    }

    /* We made it here with no errors.  Woot! */
    retval = 0;

errout:
    close(fd);    /* If fd is <0 from a failed open call, it's safe to just ignore the close error */

    return retval;
}

int list_blk_dev(void)
{
    char buffer[DM_CRYPT_BUF_SIZE];
    int retval = -1;

    int fd = open(DEVMAPPER_CONTROL_FILE, O_RDWR);
    if (fd < 0) {
        pr_error("Cannot open device-mapper\n");
        goto out;
    }

    struct dm_ioctl *io = (struct dm_ioctl *) buffer;

    ioctl_init(io, DM_CRYPT_BUF_SIZE, NULL, 0);
    if (ioctl(fd, DM_LIST_DEVICES, io)) {
        pr_error("Cannot list dm-crypt device %i\n", errno);
        goto out;
    }

    if (io->flags & DM_BUFFER_FULL_FLAG) {
        pr_error("IO buffer overflow %i\n", errno);
        goto out;
    }

    if (io->data_size == sizeof(*io)) {
        retval = 0;
        goto out;
    }

    struct dm_name_list *nl;
    nl = (struct dm_name_list *) ((void *) io + io->data_start);
	if (nl->dev) {
		while (1) {
            pr_info("%s=dm-%d\n", nl->name, minor(nl->dev));
			if (!nl->next)
				break;

			nl = (struct dm_name_list*) (((char *) nl) + nl->next);
		}
	}
    retval = 0;

out:
    close(fd);

    return retval;
}
