<?php
function formatEvent($aline) {
    if (strlen($aline) < 5) {
        return;
    }
    $pattern1='{ ^([-0-9: ]+) \s+ < ([^>]+)> \s+ \[ ([^\]]+) \] \s+ (.*) $ }xms';
    if (1 == preg_match( $pattern1, $aline, $matches)) {
        $timestamp = $matches[1];
        $level = $matches[2];
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
function formatLogFile($filename) {
    print '<table id="logevents"> <tr> <th>Date</th> <th>Level</th>' .
          ' <th>Facility</th> <th>Message</th> </tr>';
    $logfile = fopen($filename, "r") or die("Unable to open log file!");
    while(!feof($logfile)) {
        $logline = fgets($logfile);
        formatEvent( $logline );
    }
    fclose($logfile);
    print "</table>\n";
}

$logbase = "/home/sharp/";

// get the 'log' parameter from URL
$lfname = $_REQUEST["log"];
// TODO: sanitize lfname

formatLogFile( $logbase . $lfname);
?>
