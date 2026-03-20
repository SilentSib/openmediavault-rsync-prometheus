<?php
/**
 * This file is part of openmediavault-rsync-prometheus.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 */
namespace Engined\Rpc;

use OMV\Config\Database;
use OMV\Engine\Notify\Dispatcher;

class RsyncPrometheus extends \OMV\Rpc\ServiceAbstract {

    public function getName() {
        return "RsyncPrometheus";
    }

    public function initialize() {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
    }

    public function getSettings($params, $context) {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        $db = Database::getInstance();
        $object = $db->get("conf.service.rsyncp");
        $result = $object->getAssoc();
        $result["password"] = empty($result["password"]) ? "" : "**UNCHANGED**";
        return $result;
    }

    public function setSettings($params, $context) {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);
        $this->validateMethodParams($params, "rpc.rsyncp.setsettings");
        $db = Database::getInstance();
        $object = $db->get("conf.service.rsyncp");
        if ($params["password"] === "**UNCHANGED**") {
            $params["password"] = $object->get("password");
        }
        $object->setAssoc($params);
        $db->set($object);
        // Notify so the module listener marks itself dirty.
        // The Salt state runs on Apply, not here.
        $dispatcher = Dispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.conf.service.rsyncp",
            $object->getAssoc()
        );
        return $object->getAssoc();
    }
}
