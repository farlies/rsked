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
    if (! exec('./checknewsked.sh', $output, $retval) ) {
        echo("ERROR: unable to check new schedule");
    }
    else if ($retval == 0) {
        echo("accepted, version " . $sversion);
    } else {
        if (count($output) > 0) {
            echo("ERROR " . $output[0]);
        } else {
            echo("ERROR exit code " . $retval);
        }
    }
} else {
    echo("ERROR saving schedule");
}
?>
