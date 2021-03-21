//// Rsked log viewer (needs jquery.js), code for logview.html
"use strict";

var all_menus=undefined;

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
    select_log( $('#curlogs').val() );
}

function load_log_menus() {
    $.getJSON("findlogs.php","all",populate_log_menus);
    var $logsel = $('#curlogs');
    $logsel.change( function() { select_log( $('#curlogs').val() ); } );
}

function select_log( lgname ) {
    console.log( "select log: '"+lgname+"'");
    $.get("log2table.php",{log: lgname },
          function(resp) {
              $('#logarea').html(resp);
          })
}

load_log_menus();
