# /srv/salt/omv/deploy/rsync-prometheus/init.sls

{% set config      = salt['omv_conf.get']('conf.service.rsyncp') %}
{% set rsync_jobs  = salt['omv_conf.get']('conf.service.rsync.job') %}
{% set wrapper_dir = '/usr/local/lib/omv-rsync-prometheus' %}
{% set cron_script_dir = '/var/lib/openmediavault/cron.d' %}
{% set cron_file   = '/etc/cron.d/openmediavault-rsync' %}

# ── Wrapper directory ─────────────────────────────────────────────────────────

{{ wrapper_dir }}:
  file.directory:
    - user: root
    - group: root
    - mode: "0755"
    - makedirs: True

# ── Logfile ───────────────────────────────────────────────────────────────────

/var/log/omv-rsync-prometheus.log:
  file.managed:
    - user: root
    - group: root
    - mode: "0640"
    - replace: False

# ── Logrotate ─────────────────────────────────────────────────────────────────

/etc/logrotate.d/omv-rsync-prometheus:
  file.managed:
    - user: root
    - group: root
    - mode: "0644"
    - contents: |
        /var/log/omv-rsync-prometheus.log {
            weekly
            rotate 8
            compress
            delaycompress
            missingok
            notifempty
        }

{% if config and config.get('enable') and config.get('pushgateway_url') %}

# ── Per-job wrapper scripts ───────────────────────────────────────────────────

{# omv_conf.get returns a dict for a single job, list for multiple #}
{% if rsync_jobs is mapping %}
  {% set rsync_jobs = [rsync_jobs] %}
{% elif rsync_jobs is none %}
  {% set rsync_jobs = [] %}
{% endif %}

{% for job in rsync_jobs %}

{{ wrapper_dir }}/rsync-{{ job.uuid }}:
  file.managed:
    - user: root
    - group: root
    - mode: "0755"
    - source: salt://omv/deploy/rsync-prometheus/files/wrapper.sh.j2
    - template: jinja
    - context:
        config: {{ config | tojson }}
        job: {{ job | tojson }}
        cron_script_dir: "{{ cron_script_dir }}"
    - require:
      - file: {{ wrapper_dir }}

{% endfor %}

# ── Remove wrappers for jobs that no longer exist ─────────────────────────────

{% set live_uuids = rsync_jobs | map(attribute='uuid') | list | join(' ') %}

omv_rsyncp_cleanup:
  cmd.run:
    - name: |
        find {{ wrapper_dir }} -maxdepth 1 -type f -name 'rsync-*' \
          | while IFS= read -r f; do
              uuid="${f##*/rsync-}"
              case " {{ live_uuids }} " in
                *" $uuid "*) ;;
                *) rm -f "$f" ;;
              esac
            done

# ── Patch crontab to call wrappers ────────────────────────────────────────────

omv_rsyncp_patch_crontab:
  cmd.run:
    - name: |
        if [ -f {{ cron_file }} ]; then
          sed -i 's|{{ cron_script_dir }}/rsync-|{{ wrapper_dir }}/rsync-|g' \
            {{ cron_file }}
        fi
{% if rsync_jobs | length > 0 %}
    - require:
{% for job in rsync_jobs %}
      - file: {{ wrapper_dir }}/rsync-{{ job.uuid }}
{% endfor %}
{% endif %}

{% else %}

# ── Plugin disabled or not yet configured — restore direct cron calls ─────────

omv_rsyncp_restore_crontab:
  cmd.run:
    - name: |
        if [ -f {{ cron_file }} ]; then
          sed -i 's|{{ wrapper_dir }}/rsync-|{{ cron_script_dir }}/rsync-|g' \
            {{ cron_file }}
        fi

omv_rsyncp_remove_wrappers:
  file.absent:
    - name: {{ wrapper_dir }}

{% endif %}
