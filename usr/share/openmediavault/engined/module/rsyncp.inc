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
        $this->exec("omv-salt deploy run rsync-prometheus");
    }

    public function bindListeners(\OMV\Engine\Notify\Dispatcher $dispatcher) {
        // Mark dirty when our own settings are saved.
        $dispatcher->addListener(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.conf.service.rsyncp",
            [$this, "setDirty"]
        );
        // Mark dirty when rsync jobs change so wrappers stay in sync.
        $dispatcher->addListener(
            OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY | OMV_NOTIFY_DELETE,
            "org.openmediavault.conf.service.rsync.job",
            [$this, "setDirty"]
        );
    }
}
