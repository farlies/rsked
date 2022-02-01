//// Rsked log viewer (needs jquery.js), code for logview.html
"use strict";

var gScheduleWin=undefined;
var all_menus=undefined;

/// Link to schedule editor--just change focus if already open
///
function open_rcal() {
    if (undefined==gScheduleWin || gScheduleWin.closed) {
        gScheduleWin = window.open("./rcal.html","rcal");
    }
    gScheduleWin.focus();
}

function populate_log_menus(resp,status,jxref) {
    // assume status=="success" if we get here...
    all_menus = resp;
    var initial_value=undefined;
    $("#curlogs").empty();
    for (let om of all_menus.current) {
        if (om.substr(0,6)=='rsked_') {
            initial_value = 'logs/'+om;
            console.log(initial_value);
        }
        $('<option value="logs/'+om+'">'+om+'</option>').appendTo('#curlogs');
    }
    $('<option disabled>────────────────</option>').appendTo('#curlogs');
    for (let om of all_menus.old) {
        $('<option value="logs_old/'+om+'">'+om+'</option>').appendTo('#curlogs');
    }
    if (undefined!=initial_value) {
        $('#curlogs').val(initial_value);
    }
    refresh_log();
}


function refresh_log() {
    select_log( $('#curlogs').val(),
                $('#warnerroronly').prop('checked'));
}

function select_log( lgname, severe ) {
    console.log( "select log: '"+lgname+"'");
    $.get("log2table.php",
          {log: lgname, warnerroronly: severe },
          function(resp) {
              $('#logarea').html(resp);
          })
}

function load_log_menus() {
    $.getJSON("findlogs.php","all",populate_log_menus);
    var $logsel = $('#curlogs');
    $logsel.change( refresh_log );
    $('#warnerroronly').change( refresh_log );
}

load_log_menus();
