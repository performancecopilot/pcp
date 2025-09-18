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
    -p 127.0.0.1:44323:44323 \
    -v /pcp/archives/on/host:/archives \
    -v /grafana/dashboards/on/host:/dashboards \
    quay.io/performancecopilot/archive-analysis
```

This command starts the container, which runs Valkey, Grafana, the PCP REST API (pmproxy) and loads all PCP archives of the selected directory on the host into Valkey.  This directory is checked every few seconds for new archives arriving while the container is running as described at `https://man7.org/linux/man-pages/man1/pmseries_import.1.html`.

You can point your browser to `http://localhost:3000/dashboards` and perform archive analysis using Grafana.

To stop the container, run `podman rm -f pcp-archive-analysis`.

## Additional Dashboards

If the optional /dashboards volume (second -v option above) is used, additional dashboards from the host will be provisioned by Grafana from the specified directory.

## PCP REST API Access

If the optional PMWEBAPI(3) port (second -p option above) is used, you can access the REST API described at `https://pcp.readthedocs.io/en/latest/api/`.

Read access is available using the pmseries(1) query language described at `https://man7.org/linux/man-pages/man1/pmseries.1.html`

```
curl -s 'http://localhost:44323/series/query?expr=disk.dev.read*[samples:1]'
```

Write access is available using the pmlogger(1) push functionality described at `https://man7.org/linux/man-pages/man1/pmlogger.1.html`

```
pmlogconf -c config.push;
pmlogger -c config.push -t 10sec http://localhost:44323
```
