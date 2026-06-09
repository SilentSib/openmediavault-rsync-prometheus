<?php
/**
 * This file is part of openmediavault-rsync-prometheus.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 */
namespace Engined\Module;

class RsyncPrometheus extends \OMV\Engine\Module\ServiceAbstract
        implements \OMV\Engine\Notify\IListener {

    public function getName() {
        return "rsync-prometheus";
    }

    public function deployAfter(): array {
        // Run after the rsync module so the cron scripts we wrap exist first.
        return ["rsync"];
    }

    public function applyConfig() {
        // OMV 8: Start/stop systemd service directly
        if ($this->getConfig()['enable'] ?? false) {
            $this->exec("systemctl daemon-reload");
            $this->exec("systemctl enable --now omv-rsync-prometheus");
        } else {
            $this->exec("systemctl disable omv-rsync-prometheus");
            $this->exec("systemctl stop omv-rsync-prometheus");
        }
    }

    // OMV 8: No longer need bindListeners() - engined auto-reloads on config change
}
