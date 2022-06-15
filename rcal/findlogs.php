<?php
/// Return a JSON object with fields "current" and "old", each an
/// array of log names, and "site" with the current site name

/// This defines logbase for the installed system, e.g. /home/pi/logs
require('localparams.php');

/// Return an array of .log files in the directory dirname.
///
function find_logs( $dirname ) {
    $d = dir($dirname);
    // Option: restrict it to certain application logs only:
    // $lpattern = '{ ^(cooling|check_inet|rsked|vumonitor) \S* \.log $}xms';
    $lpattern = '{ ^ \S* \.(log|out) $}xms';
    $lognames = array();
    $i=1;
    while (false !== ($entry = $d->read())) {
        if (preg_match( $lpattern, $entry )) {
            array_push($lognames,$entry);
        }
    }
    $d->close();
    sort($lognames);
    return $lognames;
}
$newlogs = find_logs($logbase . "logs");
$oldlogs = find_logs($logbase . "logs_old");
$all_logs = array( "current" => $newlogs, "old"=>$oldlogs, "site"=>$sitename );
echo json_encode($all_logs) . "\n";
?>
