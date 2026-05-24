# zinit
zinit is lightweight and simple initialization system designed for zOS

## CLI
To manage zinit, use ``systemctl`` command. (``systemctl --help`` for usage)

## Services
Service files are stored in ``/etc/zinit.d`` directory. Service file looks like that:
```
name = udhcpc
type = process # can be 'process' for services that need to be restarted in case of failure, or 'oneshot' for services that need to run once (ex. 'mount /dev/sda /mnt')
command = "udhcpc -i eth0 -f" # <- '-f' needed here!!!
dependencies = mdev,mount # or 'none'
```
