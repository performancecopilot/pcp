# Performance Co-Pilot container

Performance Co-Pilot ([PCP](https://pcp.io)) is a system performance analysis toolkit.

## Usage with podman

```
$ podman run -d \
    --name pcp \
    --systemd always \
    -p 44321:44321 \
    -p 44322:44322 \
    -e PCP_DOMAIN_AGENTS=apache,uwsgi \
    -v pmlogger:/var/log/pcp/pmlogger \
    -v pmproxy:/var/log/pcp/pmproxy \
    quay.io/performancecopilot/pcp
```

**Note:** On SELinux enabled systems, the following boolean needs to be set: `sudo setsebool -P container_manage_cgroup true`

### Enabling host processes, eBPF, network and container metrics

```
$ sudo podman run -d \
    --name pcp \
    --privileged \
    --net host \
    --systemd always \
    -e HOST_MOUNT=/host \
    -e PCP_DOMAIN_AGENTS=bpf,bpftrace \
    -v pmlogger:/var/log/pcp/pmlogger \
    -v pmproxy:/var/log/pcp/pmproxy \
    -v /:/host:ro,rslave \
    quay.io/performancecopilot/pcp
```

## Usage with docker

```
$ docker run -d \
    --name pcp \
    -p 44321:44321 \
    -p 44322:44322 \
    -v pmlogger:/var/log/pcp/pmlogger \
    -v pmproxy:/var/log/pcp/pmproxy \
    -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
    registry.redhat.io/rhel8/pcp
```

**Note:** On SELinux enabled systems, the following boolean needs to be set: `sudo setsebool -P container_manage_cgroup true`

## Configuration

### Environment Variables

#### `PCP_SERVICES`
Default: `pmcd,pmie,pmlogger,pmproxy`

Comma-separated list of PCP services to start.

#### `PCP_DOMAIN_AGENTS`
Default: unset.

Comma-separated list of non-default PCP domain agents to start.

#### `HOST_MOUNT`
Default: unset.

Path inside the container to the bind mount of `/` on the host.

#### `KEY_SERVERS`
Default: `localhost:6379`

Key server (Valkey or Redis) connection spec(s) - could be any individual cluster host, and all hosts in the cluster will be automatically discovered.
Alternately, use comma-separated hostspecs (non-clustered setup)

### Configuration Files

For custom configuration options beyond the above environment variables, facilities within a container orchestration system would typically be used (Kubernetes / OpenShift ConfigMaps are very useful here).

Another possibility is to use a bind mount with a configuration file on the host to the container.
Example command to run a pmlogger-only container:

```
$ podman run -d \
    --name pmlogger \
    --systemd always \
    -e PCP_SERVICES=pmlogger \
    -v $(pwd)/pmlogger.control:/etc/pcp/pmlogger/control.d/local:z \
    -v pmlogger:/var/log/pcp/pmlogger \
    -v pmproxy:/var/log/pcp/pmproxy \
    quay.io/performancecopilot/pcp
```

pmlogger.control for local logging:
```
$version=1.1

www.example.com   n   n	PCP_ARCHIVE_DIR/www-example -N -r -T24h10m -c config.example -v 100Mb
```

pmlogger.control for remote logging:
```
$version=1.1

www.example.com   n   n	+PCP_ARCHIVE_DIR/www-example    -N -r -T24h10m -c config.example -v 100Mb http://central.example.com:44321
```

## Volumes

### `/var/log/pcp/pmlogger`

Performance Co-Pilot archive files with historical metrics from local loggers.

### `/var/log/pcp/pmproxy`

Performance Co-Pilot archive files with historical metrics from remote loggers.

## Ports

### `44321/tcp`

The pmcd daemon listens on this port and exposes the [PMAPI(3)](https://man7.org/linux/man-pages/man3/pmapi.3.html) to access metrics.

### `44322/tcp`

The pmproxy daemon listens on this port and exposes the REST [PMWEBAPI(3)](https://man7.org/linux/man-pages/man3/pmwebapi.3.html) to access metrics and/or receive remote logged archives.

## Documentation

[PCP books](https://pcp.readthedocs.io)
