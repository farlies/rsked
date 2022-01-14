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
} elseif ($reqvers == 'previous') {
    // TODO: not generally correct... fix with knownscheds.php
    $sfilepath = $cfgbase . "schedule.json.~1~";
} else {
    echo '{"error":"version unavailable"}';
    return;
}

// echo $reqvers, ": ", $sfilepath;
if (file_exists($sfilepath)) {
    $nb = readfile($sfilepath);
    if ($nb == false) {
        echo "\n{}\n";
    }
} else {
    echo "\n{}\n";
}
?>
