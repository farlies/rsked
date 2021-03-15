<?php
// Script to accept Ajax POSTed schedule and save it for installation.
// This goes in the web root html/ directory.
// Actual work of validating schedule done by shell script checknewsked.sh.
// Returns either "accepted, version ..." or "ERROR ...".
// The new schedule JSON will be in $_POST[].
//
$sked = $_POST['schedule'];
$jsked = json_decode($sked);
$sversion = $jsked->version;
$dstfile = fopen("upload/newschedule.json", "w");
if ($dstfile) {
    fwrite($dstfile,$sked);
    fclose($dstfile);
    $output=null;
    $retval=null;
    exec('./checknewsked.sh', $output, $retval);
    if ($retval == 0) {
        echo("accepted, version " . $sversion);
    } else {
        echo("ERROR " . $output[0]);
    }
} else {
    echo("ERROR saving schedule");
}
?>
