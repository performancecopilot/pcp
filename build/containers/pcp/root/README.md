# Performance Co-Pilot container

Performance Co-Pilot ([PCP](https://pcp.io)) is a system performance analysis toolkit.

## Usage with podman

```
$ podman run -d \
    --name pcp \
    --systemd always \
    -p 44321:44321 \
    -p 44322:44322 \
    -v pcp-archives:/var/log/pcp/pmlogger \
    quay.io/performancecopilot/pcp
```

**Note:** On SELinux enabled systems, the following boolean needs to be set: `sudo setsebool -P container_manage_cgroup true`

### Enabling host processes, network and container metrics

```
$ sudo podman run -d \
    --name pcp \
    --privileged \
    --net host \
    --systemd always \
    -e HOST_MOUNT=/host \
    -v pcp-archives:/var/log/pcp/pmlogger \
    -v /:/host:ro,rslave \
    quay.io/performancecopilot/pcp
```

## Usage with docker

```
$ docker run -d \
    --name pcp \
    -p 44321:44321 \
    -p 44322:44322 \
    -v pcp-archives:/var/log/pcp/pmlogger \
    -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
    registry.redhat.io/rhel8/pcp
```

**Note:** On SELinux enabled systems, the following boolean needs to be set: `sudo setsebool -P container_manage_cgroup true`

## Configuration

### Environment Variables

#### `PCP_SERVICES`
Default: `pmcd,pmie,pmlogger,pmproxy`

Comma-separated list of PCP services to start.

#### `HOST_MOUNT`
Default: unset.

Path inside the container to the bind mount of `/` on the host.

#### `KEY_SERVERS`
Default: `localhost:6379`

Key server connection spec(s) - could be any individual cluster host, and all hosts in the cluster will be automatically discovered.
Alternately, use comma-separated hostspecs (non-clustered setup)

### Configuration Files

For custom configuration options beyond the above environment variables, it is advised to use a bind mount with a configuration file on the host to the container.
Example command to run a pmlogger-only container:

```
$ podman run -d \
    --name pmlogger \
    --systemd always \
    -e PCP_SERVICES=pmlogger \
    -v $(pwd)/pmlogger.control:/etc/pcp/pmlogger/control.d/local:z \
    -v pcp-archives:/var/log/pcp/pmlogger \
    quay.io/performancecopilot/pcp
```

pmlogger.control:
```
$version=1.1

remote.pmcdhost.corp	n   n	PCP_ARCHIVE_DIR/remote_pmcd	-N -r -T24h10m -c config.default -v 100Mb
```

## Volumes

### `/var/log/pcp/pmlogger`

Performance Co-Pilot archive files with historical metrics.

## Ports

### `44321/tcp`

The pmcd daemon listens on this port and exposes the [PMAPI(3)](https://man7.org/linux/man-pages/man3/pmapi.3.html) to access metrics.

### `44322/tcp`

The pmproxy daemon listens on this port and exposes the REST [PMWEBAPI(3)](https://man7.org/linux/man-pages/man3/pmwebapi.3.html) to access metrics.

## Documentation

[PCP books](https://pcp.readthedocs.io)
