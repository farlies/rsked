<?php
/// getschedule.php
///
/// Fetch the indicated schedule (current or previous) as json text.
/// Optional query parameter 'version' can be either 'current' or 'previous'
/// Default is 'current'.

require('localparams.php');

$reqvers = "current";
if (array_key_exists("version",$_REQUEST)) {
    $reqvers = $_REQUEST["version"];
}

if (0==strlen($reqvers) or $reqvers=="current") {
    $sfilepath = $cfgbase . "schedule.json";
    readfile($sfilepath);
    // TODO: handle case of reqvers=='previous';
} else {
    echo '{"error":"version unavailable"}';
}
?>
