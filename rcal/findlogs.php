<?php
/// This defines logbase for the installed system.
require('localparams.php');

/// return json obj with fields current and old, each an array of log names
///
function find_logs( $dirname ) {
    $d = dir($dirname);
    $lpattern = '{ ^(cooling|check_inet|rsked|vumonitor) \S* \.log $}xms';
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
$all_logs = array( "current" => $newlogs, "old"=>$oldlogs );
echo json_encode($all_logs) . "\n";
?>
