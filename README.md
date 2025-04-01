# Lightweight device-mapper utils

- List device, output \<device name\> = \<block device\>
```bash
> dm-utils ls
dev0=dm-0
dev1=dm-1
```

- Delete device
```bash
> dm-utils del -h
Device-mapper utils.

 [-h] del -d <string>
  del                            Delete device
  -d, --dev=<device>             Device name
  -h, --help                     Print this help and exit

> dm-utils del -d dev0
```

- Create crypt device, check https://docs.kernel.org/admin-guide/device-mapper/dm-crypt.html for the meaning of fields.
```bash
> dm-utils crypt -h
Device-mapper utils.

 [-h] crypt -d <device> -c <cipher> -k <key>> -b <device path> [-o <opt_params>]...
  crypt                          Create crypt device
  -d, --dev=<device>             Device name
  -c, --cipher=<cipher>          Encryption cipher
  -k, --key=<key>>               Encryption key
  -b, --blockdev=<device path>   Block device path
  -o, --opt=<opt_params>         Optional parameter
  -h, --help                     Print this help and exit

> dm-utils crypt -d dev1 -c aes-cbc-essiv:sha256 -k babebabebabebabebabebabebabebabe -b /dev/loop0 -o same_cpu_crypt -o sector_size:4096
```
