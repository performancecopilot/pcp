# archive-analysis container
This container bundles Performance Co-Pilot, Redis, Grafana and grafana-pcp.
All services are preconfigured and PCP archives mounted at `/archives` will be imported at startup and on every change.

## Usage
```
$ podman run \
    --name pcp-archive-analysis \
    -t --rm \
    --security-opt label=disable \
    -p 3000:3000 \
    -v /location/to/pcp/archives/on/host:/archives \
    quay.io/performancecopilot/archive-analysis
```

Open Grafana at `http://localhost:3000`.

To stop the container, run `podman rm -f pcp-archive-analysis`.
