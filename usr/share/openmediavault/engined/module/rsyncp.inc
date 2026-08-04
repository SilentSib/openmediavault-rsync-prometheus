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
        // Run after the rsync module so the cron scripts we wrap exist,
        // and after the crontab has been rewritten by any other change,
        // so our patch is always applied last.
        return ["rsync"];
    }

    public function applyConfig() {
        $this->exec("omv-salt deploy run rsync-prometheus");
    }

    /**
     * Mirrors OMV core's rsync module onSharedFolder listener: if the
     * shared folder that changed is referenced by any rsync job (as
     * either src or dest), mark ourselves dirty too. Without this,
     * any change to a shared folder — including ones made via external
     * tools like Terraform — causes the core rsync module to rewrite
     * /etc/cron.d/openmediavault-rsync from scratch, silently reverting
     * our wrapper patch since we'd otherwise never be re-triggered.
     */
    final public function onSharedFolder($type, $path, $object) {
        $db = \OMV\Config\Database::getInstance();
        if (TRUE === $db->exists("conf.service.rsync.job", [
                "operator" => "or",
                "arg0" => [
                        "operator" => "stringEquals",
                        "arg0" => "src",
                        "arg1" => $object['uuid']
                ],
                "arg1" => [
                        "operator" => "stringEquals",
                        "arg0" => "dest",
                        "arg1" => $object['uuid']
                ]
        ])) {
            $this->setDirty();
        }
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

        // Mark dirty when a shared folder referenced by an rsync job
        // changes — same trigger the core rsync module uses.
        $dispatcher->addListener(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.conf.system.sharedfolder",
            [$this, "onSharedFolder"]
        );
    }
}
