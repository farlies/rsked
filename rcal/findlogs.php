<?php
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
$newlogs = find_logs("/home/sharp/logs");
$oldlogs = find_logs("/home/sharp/logs_old");
$all_logs = array( "current" => $newlogs, "old"=>$oldlogs );
echo json_encode($all_logs) . "\n";
?>
