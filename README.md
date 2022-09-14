# netreactd

A small C program that runs a script if no address is given to an interface after enough time has elapsed.

## Installation

1. Install grab a [pre-build binary](https://github.com/Airtonomy/netreactd/releases) or build from source
2. Install that file to `/usr/local/bin/netreactd`
3. `chown root:root /usr/local/bin/netreactd`
4. `chmod 0755 /usr/local/bin/netreactd`

## Systemd File

1. Create a new file at `/etc/systemd/system/netreactd-lidar.service`
2. Add the following contents:

```systemd
[Unit]
Description=Monitor connections on eth0 and setup a static IP to Lidar when DHCP doesn't kick in
Before=network-pre.target
Wants=network-pre.target
[Service]
Type=simple
Restart=always
RestartSec=2s
Environment=NETREACT_IF=eth0
Environment=NETREACT_TIMEOUT=4
Environment=NEWLINK_SCRIPT='/abs/path/to/toNewLinkScript'
Environment=NETREACT_UP_SCRIPT='/abs/path/to/toUpScript'
Environment=NETREACT_DOWN_SCRIPT='/abs/path/to/toDownScript'
Environment=SKIP_FIRST_NEW_ADDRESS='1'
ExecStart=/usr/local/bin/netreactd

[Install]
WantedBy=network.target
```

## Build from source

1. Clone the repository
2. make
