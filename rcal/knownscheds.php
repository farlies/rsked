<?php
/// Compile a list of the available schedule versions...
/// Backup schedules have names like schedule.json.~1~
/// where higher numbers indicate more recent backups.
/// emits a json object with the following attributes:
///   status:   'ok' or 'error'
///   versions:  [array of version filenames]

require('localparams.php');

$canonical = "schedule.json";
$nc = strlen($canonical);

if (!is_dir($cfgbase)) {
    echo "{'status' : 'error', 'message': '",$cfgbase," is not a readable directory'}\n";
    return;
}

/// Return the backup id number from the file name. Returns 1e6 if file
/// has no suffix; return a negative number if not a numbered backup.
///
function extract_bid( $file, $nc )
{
    $nf = strlen($file);
    if ($nf == $nc) {
        return 1000000; // establishes as most recent, probably
    }
    $id = 0;
    $j = $nf - 1;
    if ("~" == substr($file,$j,1)) { // final char
        $j = ($j - 1);
        $ids = "";
        while ($j > 0) {
            $d = substr($file,$j,1);
            if (ctype_digit($d)) {
                $ids = $d . $ids;
                $j = ($j - 1);
            } else {
                break;
            }
        }
        if (0 == strlen($ids)) { return 'invalid'; }
        $id = intval($ids); // as integer
    }
    return $id;
}

///////////////////////

$vers = array();

if ($dh = opendir($cfgbase)) {
    while (($file = readdir($dh)) !== false) {
        if (0 == strncmp($file, $canonical, $nc)) {
            $id = extract_bid( $file, $nc );
            if ($id != 'invalid') { $vers[$id] = $file; }
        }
    }
    closedir($dh);
}

/// sort from newest to oldest
krsort($vers);

// emit as json array
if (0==count($vers)) {
    echo "{'status':'ok','versions':[ ]}\n";
} else {
    echo "{'status':'ok','versions':['",implode("','",$vers),"']}\n";
}

?>
