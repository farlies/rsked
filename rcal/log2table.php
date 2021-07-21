<?php
/// This file, which is not part of the distribution for rsked/rcal,
/// should define the variable $logbase, a directory ending in '/' e.g.
///      $logbase="/home/pi/";
///
require('localparams.php');

function formatEvent($aline,$severe) {
    if (strlen($aline) < 5) {
        return;
    }
    $pattern1='{ ^([-0-9: ]+) \s+ < ([^>]+)> \s+ \[ ([^\]]+) \] \s+ (.*) $ }xms';
    if (1 == preg_match( $pattern1, $aline, $matches)) {
        $timestamp = $matches[1];
        $level = $matches[2];
        if ($severe and (($level == "info") or ($level == "debug"))) {
            return;
        }
        $facility = $matches[3];
        $message = trim($matches[4]);
        echo '<tr class="'. $level . '"><td>' . $timestamp . '</td><td>' .
             $level . '</td><td>' . $facility . '</td><td>' .
             $message . '</td></tr>' . "\n";
    } else {
        echo '<tr class="misc"><td colspan="4">' . trim($aline) . "</td></tr>\n";
    }
}

/* formats filename as a table */
function formatLogFile($filename, $severe) {
    print '<table id="logevents"> <tr> <th>Date</th> <th>Level</th>' .
          ' <th>Facility</th> <th>Message</th> </tr>';
    $logfile = fopen($filename, "r") or die("Unable to open log file!");
    while(!feof($logfile)) {
        $logline = fgets($logfile);
        formatEvent( $logline, $severe );
    }
    fclose($logfile);
    print "</table>\n";
}


// get the 'log' parameter from URL
$lfname = $_REQUEST["log"];
$severe = ("true" == $_REQUEST["warnerroronly"]);
// TODO: sanitize lfname

formatLogFile( $logbase . $lfname, $severe);
?>
