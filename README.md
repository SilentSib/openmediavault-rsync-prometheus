# openmediavault-rsync-prometheus

An [OpenMediaVault 7](https://www.openmediavault.org/) plugin that wraps every rsync cronjob to push **success/failure metrics** to a [Prometheus Pushgateway](https://github.com/prometheus/pushgateway) after each run — without touching OMV's own scripts or configuration.

Completely built using Claude's free tier (Sonnet 4.6).

Not everything has been fully tested yet but the main parts look solid.

## Raison d'être of this plugin

I love OMV. I do. And the rsync cronjobs are a core feature in my usage of the software. So much so that I've been wanting some level of prometheus integration for a while. The emails are nice but I wanted alerts to fire when something was not working instead of manually checking the body of each email every day. That's why I wanted something like this.

But because I have no time to dive deeply in the internals, I figured that a LLM could maybe help, which it did.

Also, one might argue it would be better to have this directly added to the core feature but 1) I didn't think it made sense unless the whole software was updated with this in mind and 2) I'd have no time to do this, even with a LLM.

---

## Metrics

Four gauges are pushed per job after every run:

| Metric | Description |
|---|---|
| `omv_rsync_job_success` | `1` = last run succeeded, `0` = failed |
| `omv_rsync_job_last_exit_code` | Derived exit code (`0` = success, `1` = OMV reported failure) |
| `omv_rsync_job_duration_ms` | Wall-clock duration of the run in milliseconds |
| `omv_rsync_job_last_run_timestamp_seconds` | Unix timestamp of run completion |

All metrics carry three labels:

| Label | Value |
|---|---|
| `job` | `<prefix>_<uuid>` e.g. `omv_rsync_2e0188a7_88a5_...` |
| `instance` | Hostname (or custom override from settings) |
| `name` | Human-friendly name: the job's **Comment** field, or UUID if no comment is set |

> **Tip:** set a Comment on each rsync job in the OMV UI (`Services › Rsync › Edit`) to get readable `name` labels like `"TV Shows"` or `"Nightly Backup"`.

---

## How it works

OMV writes one cron line per rsync job to `/etc/cron.d/openmediavault-rsync`:

```
0 2 * * *  root  /var/lib/openmediavault/cron.d/rsync-<uuid>  2>&1 | mail ...
```

This plugin patches that file so each line calls a **wrapper script** instead:

```
0 2 * * *  root  /usr/local/lib/omv-rsync-prometheus/rsync-<uuid>  2>&1 | mail ...
```

Each wrapper:
1. Runs the original OMV rsync script and captures its output
2. Detects failure by checking for OMV's exact failure string (`"synchronisation failed"`) — because OMV always exits `0` even on failure to avoid duplicate cron emails
3. Pushes the four metrics to the Pushgateway via `curl`
4. Logs the result to `/var/log/omv-rsync-prometheus.log`
5. Exits `0` (matching OMV's own behaviour so cron/mail still works normally)

The original OMV scripts are **never modified**.

### Architecture

```
OMV cron daemon
  └─ /etc/cron.d/openmediavault-rsync         ← patched to call wrappers
        │
        ▼
  /usr/local/lib/omv-rsync-prometheus/rsync-<uuid>   (generated wrapper)
    ├── runs /var/lib/openmediavault/cron.d/rsync-<uuid>   (original OMV script)
    ├── detects success/failure from stdout
    ├── records duration
    └── POSTs metrics ──────────────────────► Prometheus Pushgateway
                                                      │
                                                      ▼
                                               Prometheus scrapes
                                                      │
                                                      ▼
                                                  Grafana
```

### OMV integration layer

| Component | Purpose |
|---|---|
| `datamodels/conf.service.rsyncp.json` | Config schema stored in `config.xml` |
| `datamodels/rpc.rsyncp.setsettings.json` | RPC parameter validation |
| `engined/rpc/rsyncp.inc` | `getSettings` / `setSettings` RPC methods |
| `engined/module/rsyncp.inc` | Dirty-flag listener; runs Salt state on Apply |
| `confdb/create.d/conf.service.rsyncp.sh` | Seeds `config.xml` defaults on install |
| `workbench/` YAML files | Settings page under `Services › Rsync Metrics` |
| `srv/salt/omv/deploy/rsync-prometheus/` | Salt state that generates wrappers and patches crontab |

---

## Requirements

- OpenMediaVault 7.x (Sandworm)
- `curl` (installed automatically as a package dependency)
- A running [Prometheus Pushgateway](https://github.com/prometheus/pushgateway) reachable from the OMV host

---

## Installation

### Build from source

On a Debian 12 build machine:

```bash
git clone https://github.com/SilentSib/openmediavault-rsync-prometheus
cd openmediavault-rsync-prometheus
dpkg-buildpackage -us -uc -b
```

Copy the resulting `.deb` to your OMV host and install:

```bash
sudo dpkg -i openmediavault-rsync-prometheus_1.0.0_all.deb
sudo apt-get install -f   # resolve any missing dependencies
```

### Manual installation (without building a .deb)

```bash
# Extract all files to the root filesystem
sudo tar -xzf openmediavault-rsync-prometheus.tar.gz \
  --strip-components=1 \
  --exclude='debian' \
  --exclude='README.md' \
  -C /

# Seed config.xml with defaults
sudo omv-confdbadm create "conf.service.rsyncp"

# Compile the workbench UI
sudo omv-mkworkbench all

# Restart the RPC engine to load the new PHP files
sudo systemctl restart openmediavault-engined

# Sync Salt execution modules and apply the state
sudo salt-call --local saltutil.sync_modules
sudo omv-salt deploy run rsync-prometheus
```

---

## Configuration

Go to **Services › Rsync Metrics** in the OMV web interface.

| Field | Description | Default |
|---|---|---|
| **Enabled** | Toggle metric reporting on/off | off |
| **Pushgateway URL** | Base URL e.g. `http://192.168.1.10:9091` | — |
| **Verify TLS certificate** | Uncheck for self-signed certificates | on |
| **Username** | HTTP Basic Auth username (leave empty if unauthenticated) | — |
| **Password** | HTTP Basic Auth password | — |
| **Job label prefix** | Prefix for the Prometheus `job` label | `omv_rsync` |
| **Instance label** | Override the `instance` label (defaults to system hostname) | — |

Click **Save**, then **Apply**. OMV will regenerate the wrapper scripts and patch the crontab automatically.

### Giving jobs friendly names

The `name` label uses the **Comment** field of each rsync job. Edit a job under **Services › Rsync**, fill in the Comment field, save, and apply. The wrappers are regenerated immediately with the new name — no reinstall needed.

---

## Testing

### Test Pushgateway connectivity

```bash
sudo /usr/share/openmediavault/scripts/omv-rsync-prometheus-test
```

Pushes a single test gauge (`omv_rsync_plugin_test`) to verify that the URL, authentication, and TLS settings are correct before the next scheduled run.

### Run a wrapper manually

```bash
sudo /usr/local/lib/omv-rsync-prometheus/rsync-<uuid>
tail -f /var/log/omv-rsync-prometheus.log
```

### Inspect generated wrappers

```bash
# List all wrappers
ls -la /usr/local/lib/omv-rsync-prometheus/

# Confirm crontab is patched
grep rsync-prometheus /etc/cron.d/openmediavault-rsync

# Check stored config
omv-confdbadm read "conf.service.rsyncp"
```

---

## Logs

Every push attempt is logged to `/var/log/omv-rsync-prometheus.log`:

```
2026-03-20 22:27:33 OK        job=omv_rsync_2e0188a7_... name=TV Shows  rsync_exit=0 duration=3641ms
2026-03-20 23:00:01 FAIL      job=omv_rsync_abbd6aab_... name=Photos    rsync_exit=1 duration=812ms
2026-03-20 23:00:04 PUSH_FAIL job=omv_rsync_5a9bf2c7_... curl_exit=7    output=Connection refused
```

| Entry | Meaning |
|---|---|
| `OK` | rsync succeeded, metrics pushed |
| `FAIL` | rsync failed (OMV reported "synchronisation failed"), `success=0` pushed |
| `PUSH_FAIL` | rsync ran but the curl push to the Pushgateway failed |

Logs are rotated weekly, keeping 8 weeks of history (`/etc/logrotate.d/omv-rsync-prometheus`).

---

## Prometheus configuration

```yaml
scrape_configs:
  - job_name: omv_rsync
    honor_labels: true        # preserve job/instance labels from Pushgateway
    static_configs:
      - targets: ["192.168.1.10:9091"]
```

### Example alert rules

```yaml
groups:
  - name: omv_rsync
    rules:
      - alert: RsyncJobFailed
        expr: omv_rsync_job_success == 0
        for: 0m
        labels:
          severity: warning
        annotations:
          summary: "rsync job failed on {{ $labels.instance }}"
          description: >
            Job "{{ $labels.name }}" failed on {{ $labels.instance }}.

      - alert: RsyncJobNotRunning
        expr: time() - omv_rsync_job_last_run_timestamp_seconds > 90000
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "rsync job {{ $labels.name }} hasn't run in 25 hours"
          description: >
            Job "{{ $labels.name }}" on {{ $labels.instance }} last ran
            {{ $value | humanizeDuration }} ago.
```

---

## Grafana dashboard

A pre-built dashboard is included as `omv-rsync-prometheus-dashboard.json`.

**Import:** Grafana → Dashboards → Import → Upload JSON file → select your Prometheus datasource.

The dashboard includes:

- **Overview row** — stat panels: jobs OK, jobs failing, total tracked, average duration, max duration
- **Job Status row** — horizontal LCD bar gauge per job, green = OK / red = FAIL
- **Duration row** — time series bar chart of per-job duration over the selected time range
- **Job Details table** — one row per job: Name, Instance, Status (colour-coded), Exit Code, Duration, Last Run (relative)

An **Instance** variable at the top lets you filter by host — useful if the plugin runs on multiple OMV servers.

---

## Caveats

### OMV always exits 0

OMV's rsync scripts deliberately exit `0` even on failure. They handle user notification themselves (syslog + email) to avoid duplicate cron failure emails. This plugin detects failure by parsing stdout for OMV's exact string `"synchronisation failed"` rather than relying on the exit code.

### Crontab is re-patched automatically on every Apply

When you click Apply in the OMV UI, OMV rewrites `/etc/cron.d/openmediavault-rsync` from scratch, restoring direct calls to the raw scripts. The plugin's module listener detects this and re-applies the Salt state (re-patching the crontab) as part of the same Apply operation, so the patch is always restored before the next scheduled run.

### UI "Run" button bypasses wrappers

The **Run** button in `Services › Rsync` calls the raw OMV script directly via `sudo`, bypassing the crontab entirely. Metrics are only pushed for **scheduled cron runs**, not manual UI-triggered runs.

### Pushgateway retains last value indefinitely

Pushgateway stores the last pushed value until it is explicitly deleted or the process restarts. Use the `omv_rsync_job_last_run_timestamp_seconds` metric to detect jobs that haven't run recently, rather than relying on metric absence.

---

## Uninstallation

```bash
sudo dpkg --purge openmediavault-rsync-prometheus
```

Removes all plugin files, deletes wrapper scripts, removes the config node from `config.xml`, restores the crontab to direct rsync calls, rebuilds the workbench UI, and restarts `openmediavault-engined`.

---

## File layout

```
/usr/share/openmediavault/
  confdb/create.d/
    conf.service.rsyncp.sh                      config.xml seed script
  datamodels/
    conf.service.rsyncp.json                    config schema
    rpc.rsyncp.setsettings.json                 RPC param schema
  engined/
    rpc/rsyncp.inc                              RPC service (PHP)
    module/rsyncp.inc                           Module listener (PHP)
  scripts/
    omv-rsync-prometheus-test                   Connectivity test CLI
  workbench/
    component.d/omv-services-rsync-prometheus-settings-form-page.yaml
    navigation.d/90-services-rsync-prometheus.yaml
    route.d/services.rsync-prometheus.yaml

/srv/salt/omv/deploy/rsync-prometheus/
  init.sls                                      Salt state
  files/wrapper.sh.j2                           Wrapper script Jinja2 template

/usr/local/lib/omv-rsync-prometheus/            Generated per-job wrapper scripts
/var/log/omv-rsync-prometheus.log               Push log
/etc/logrotate.d/omv-rsync-prometheus           Log rotation config
```

---

## License

GNU General Public License v3 or later.

---

## Known issues

/