# Ad-hoc archive analysis container
This container bundles Performance Co-Pilot, Valkey, Grafana and grafana-pcp.
All services are preconfigured and PCP archives mounted at `/archives` will be imported at startup and on every change.

## Usage
```
$ podman run \
    --name pcp-archive-analysis \
    -t --rm \
    --security-opt label=disable \
    -p 127.0.0.1:3000:3000 \
    -v /location/to/pcp/archives/on/host:/archives \
    quay.io/performancecopilot/archive-analysis
```

This command starts the container, which runs Valkey and Grafana (inside the container) and loads all PCP archives of the selected directory on the host into Valkey. You can point your browser to `http://localhost:3000/dashboards` and can start inspecting the archives using Grafana. 

To stop the container, run `podman rm -f pcp-archive-analysis`.

## Additional Dashboards
Additional dashboards from the host can be provisioned by adding:
```
-v /location/to/dashboards/on/host:/dashboards
```
to the `podman run` command above.
