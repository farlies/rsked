//////////////////////////////////////////////////////////////////////////////
//// Rcal Calendar Editor

"use strict";

///   C O N S T A N T S

/// Canonical name of the host library
const gHostLibraryResource = "catalog.json";

/// Canonical name of the current schedule.
//const gScheduleResource = "schedule.json";
const gScheduleResource = "getschedule.php";

const DefaultSrcName='(new name)';

/// Names of days used in schedule.json
const DayNames = ["sunday","monday","tuesday","wednesday","thursday",
                  "friday","saturday"];

/// special color for announcments on the calendar
//const AnnColor = "#663399";
const AnnColor = "#101010";


///   G L O B A L S

/// The source being edited, if any; otherwise undefined.
var EditSource=undefined;

/// Global objecst used as map of sources and announcements by their
/// unique names, "cms" => <RcalSource cms>
///
var Sources = {};
var Announcements = {};

/// Global with the last loaded schedule
var LoadedSchedule;
var SavedSchedule;
var ScheduleModified=false;

/// Global with the installed music library catalog
var HostLibrary;

/// Global for the calendar object
var TheCalendar;

/// Base the viewable week on a certain date: 2020-03-01 This will
/// never include the DST switch (which is on the second Sunday,
/// Mar. 8).
const gDay0Str = '2020-03-01'; // a Sunday, day 0
const gYear0  = 2020;  // year 2020
const gMonth0 = 2;     // March==2
const gDay0   = 1;     // Day 1
const gDate0 = new Date(gYear0, gMonth0, gDay0, 0, 0, 0);

//////////////////////////////////////////////////////////////////////////////
////                          HOST LIBRARY

/// Accessor: Retrieve the playlists available on the host (object).
///
function host_playlists() {
    return HostLibrary.playlists;
}

/// Accessor: retrieve the recorded audio resources on the host
/// (nested Objects).
///
function host_res() {
    return HostLibrary.library;
}

/// Return an array containing strings for the first extant file resource:
/// [artist,album,track,enc].  This is used as a default in the source
/// dialog.
///
function default_lib_resource() {
    var da = Object.getOwnPropertyNames( HostLibrary.library )[0];
    var dad = HostLibrary.library[da];
    var dd = Object.getOwnPropertyNames( dad )[0];
    var alb = dad[ dd ];
    var dt = alb.tracks[0];
    var den = alb.encoding;
    return [da,dd,dt,den];
}


/// Accessor: return the album resource for a given artist.
/// It will return undefined if there is no such artist/album.
///
function get_host_album(artist,album) {
    const res=host_res();
    return res[artist][album];
}


/// Loader. Read the host library from the server src url. Upon
/// completion emit a "libraryLoaded" event to initialize the UI,
/// handled below.
///
function load_host_library( src ) {
      $.getJSON( src, function(data) {
          HostLibrary = data;
          $(document).trigger('libraryLoaded'); });
}

/// If the library loaded, fill in the resource menus, load the schedule.
///
$(document).on('libraryLoaded',
        function() {
                if ("object" === typeof(HostLibrary)) {
                    init_modals();
                    init_source_pane();
                    load_schedule( gScheduleResource );
                } else {
                    alert('failed to load host library.');
                }} );

///////////////////////////////////////////////////////////////////////////////

/// Generate a uuid (copied from GitHub gist under dwtfuw license.)
///
function uuidv4() {
  return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
    (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
  );
}

/// Assign a COLOR based on medium, one of:
///  ["radio","stream","playlist","file","directory"]
///
function medium_to_color( medium ) {
    switch( medium ) {
    case 'radio':   return "#0066ff";
    case 'stream':  return "#cc33ff";
    case 'file':    return "#009900";
    case 'playlist': return "#ff6600";
    case 'directory': return  "#996633";
    default: return "#ffcc66"; // yellow matter custard
    }
}

/// return the number of seconds into the local day of the string time
/// e.g. for "03:10:12" it would be (3*60 + 10)*60 + 12
///
/*
function getDaySecs( dstr ) {
    const ta = dstr.split(':').map( s => { return Number.parseInt(s); } );
    return ta[2] + 60*( ta[1] + 60*ta[0] );
}
*/

/// return the number of seconds into the local day of JS Date object
/// e.g. for "2020-05-01 03:10:12" it would be (3*60 + 10)*60 + 12
///
function getDaySecsDate( d ) {
    return d.getSeconds() + 60*( d.getMinutes() + 60*d.getHours() );
}


/// return a short string describing a time interval of /dur_secs/ seconds
/// in hrs/mins/secs following informal conventions. Fractions of seconds ignored.
///    >= 60 minutes:     N hr {M min}
///    >= 1 minute:       M min {S sec}
///    < 1 minute:        S sec
///
function timeIntStr( dur_secs )
{
    const dsecs = Math.round(dur_secs);   // ignore fractional seconds
    if (dsecs >= 3600) {
        const hrs = Math.floor( dsecs / 3600);
        const min = Math.round( (dsecs - 3600*hrs)/60 );
        let v = hrs+" hr";
        if (min > 0) {
            v += " "+min+" min";
        }
        return v;
    }
    if (dsecs >= 60) {
        const min = Math.floor( dsecs / 60);
        const sec = (dsecs - 60*min);
        let v = min+" min";
        if (sec > 0) {
            v += " "+sec+" sec";
        }
        return v;
    }
    return dsecs+" sec";   
}

/// Adds seconds to a Date object, returning a new Date.
/// TODO: option to stop at midnight boundary.
///
function dateAddSecs(date, secs) {
    if(!(date instanceof Date)) {
        return undefined;
    }
    var ret = new Date(date);
    ret.setTime(ret.getTime() + secs*1000);
    return ret;
}

/// Given a linux pathname, return an object with properties:
/// - name:  base name including extension, e.g. 'foobar.mp3'
/// - directory: trailing slash is removed, e.g. '/foo/bar'
/// - extension: leading '.' is removed, e.g. 'mp3'
///
function split_pathname (pstr) {
    var filenameWithExtension = pstr.replace(/^.*[\\\/]/, '');
    var directoryOnly = pstr.replace(/[^\/]*$/, '');
    var dottedWords = filenameWithExtension.split('.');
    if (directoryOnly.endsWith("/")) {
        directoryOnly = directoryOnly.slice(0,-1);
    }
    return { name: filenameWithExtension,
             extension: (dottedWords.length > 1) ? dottedWords.pop() : '',
             directory: directoryOnly };
}


/// Model for sources (OOP paradigm needs ES6 support in browser).
/// Assign a uuid to each source on construction to allow unambiguous
/// references even if the source name subsequently changes.
/// The source is initially "unregistered", meaning it is not in the
/// list of Sources.
///
class RcalSource {
    name;        // string name, unique in schedule
    suid;        // string uuid--unique in session; not in schedule
    registered;  // bool
    color;       // dom color spec.
    type;        // medium: 'radio|stream|file|playlist|directory'
    repeat;      // bool
    dynamic;     // bool
    announcement; // bool
    ann_location; // string
    alternate;   // string name of alt source
    encoding;    // 'ogg','mp3','mp4','wfm','mixed', 'flac'
    text;        // text (announcements) e.g. "good morning"
    duration;    // optional duration, seconds
    // location variants:
    artist;      // e.g. "Miles Davis"
    album;       // e.g. "Big Fun-Disc 1"
    file;        // e.g. "02-Ife.ogg"
    freq;        // e.g. 99.5
    playlist;    // e.g. "master.m3u"
    url;         // e.g. "http://foo.bar.com/fubar.mp3"
    //
    // CTOR for RcalSource. Mandatory: name,medium,encoding,resource.
    // Fields are given defaults suitable for the edit dialog.
    //
    constructor(sname,medium,encoding,sres,repeat=true,
                dynamic=false,announcement=false) {
        var pfile;
        var parts;
        this.name = sname;
        this.registered = false;
        this.suid = uuidv4();
        this.color = medium_to_color(medium);
        this.type = medium;
        this.encoding = encoding;
        this.text = undefined;
        this.duration = undefined;
        this.repeat = repeat;
        this.dynamic = dynamic;
        this.announcement = announcement;
        this.alternate = "OFF";
        this.freq = 101.1;
        this.url = 'http://some.host.com/stream.mp3';
        switch( medium ) {
        case 'radio':
            if (isNaN(sres)) {
                throw new TypeError("Invalid frequency: "+sres);
            }
            this.freq = sres;
            break;
        case 'stream':
            this.url = sres;
            break;
        case 'file':
            parts = sres.split('/');
            this.artist = parts[0];
            this.album = parts[1];
            this.file = parts[2];
            break;
        case 'directory':
            parts = sres.split('/');
            this.artist = parts[0];
            this.album = parts[1];
            break;
        case 'playlist':
            this.playlist = sres;
            break;
        default:
            throw new Error("Unknown src medium: "+medium);
        }
    }
};


// search DOM for the source (inner div) containing text sname
// return the DOM element or undefined if not found
function find_fc_source(suid) {
    let srcs = $('#'+suid,'#external-events-list');
    if (srcs.length > 0) {
        return srcs[0];
    } else {
        console.error("Cannot find an fc-event-main div for ",suid);
        return undefined;
    }
}

// Listener: respond to change of Source medium, e.g. radio -> stream
// by updating the modal dialog fields' visibility, showing only the
// relevant fields for the type.
//
function src_medium_change(event) {
    if (undefined === EditSource) { return; }
    let neumedium = event.target.value;
    //console.info('src_medium_change:', neumedium);
    const neucolor = medium_to_color(neumedium);
    $('#scolor').prop('value',neucolor);
    $('#sheader')[0].style.backgroundColor = neucolor;
    config_src_fields(neumedium);
}

/// Listener for source color change.
function src_col_change(event) {
    if (undefined === EditSource) { return; }
    const neucol = event.target.value;
    const mdheader = document.getElementById('sheader');
    mdheader.style.backgroundColor = neucol;
}

/// A more extensive URL checker from devshed forum.
///
function validURL(str) {
  var pattern = new RegExp('^(https?:\\/\\/)?'+ // protocol
    '((([a-z\\d]([a-z\\d-]*[a-z\\d])*)\\.)+[a-z]{2,}|'+ // domain name
    '((\\d{1,3}\\.){3}\\d{1,3}))'+ // OR ip (v4) address
    '(\\:\\d+)?(\\/[-a-z\\d%_.~+]*)*'+ // port and path
    '(\\?[;&a-z\\d%_.~+=-]*)?'+ // query string
    '(\\#[-a-z\\d_]*)?$','i'); // fragment locator
  return !!pattern.test(str);
}

/// Are Source Edit contents consistent enough to allow it to be saved?
///
function validate_src_dialog() {
    if (undefined === EditSource) { return; }
    //
    if (! EditSource.registered) {
        const inp=$('#sname')[0];
        const sn=inp.value;
        if (sn.length==0 || sn==DefaultSrcName) {
            inp.value = DefaultSrcName;
            inp.select();
            inp.focus();
            return false;
        }
        if (sn in Sources) {
            inp.select();
            inp.focus();
            return false;
        }
    }
    //
    const smedium = $('#smedium').prop('value');
    //console.info("Medium check for",smedium);
    if (smedium == 'stream') {
        const su=$('#iurl')[0];
        if ( ! su.validity.valid ) {
            su.focus();
            return false;
        }
    }
    if (smedium == 'radio') {
        const sf=$('#sfreq')[0];
        if ( ! sf.validity.valid ) {
            sf.focus();
            return false;
        }
    }
    if ((smedium == 'directory') || (smedium == 'file')) {
        // verify artist and album fields for directory or file
        const artist_sel = $('#sartselect')[0];
        //console.info("Artist selected:",artist_sel.selectedIndex); // DEBUG
        if ( -1 == artist_sel.selectedIndex ) {
            console.warn("You must select an artist");
            artist_sel.focus();
            return false;
        }
        const album_sel = $('#salbselect')[0];
        if ( -1 == album_sel.selectedIndex ) {
            console.warn("You must select an album");
            album_sel.focus();
            return false;
        }
        if (smedium == 'file') {
            const track_sel = $('#strackselect')[0];
            if ( -1 == track_sel.selectedIndex ) {
                console.warn("You must select a track");
                track_sel.focus();
                return false;
            }
        }
    }
    // ... other checks
    return true;
}


// listener for artist change; update album select options
function src_artist_change(event) {
    if (undefined === EditSource) { return; }
    const neuart = event.target.value;
    init_src_albums( neuart );
}

/// Init source dialog for a new default library resource
/// by setting default field values for artist/album/track.
///
function config_new_file_res() {
    if (undefined === EditSource) { return; }
    //console.info("Initialzing default local resource")
    const tetra = default_lib_resource(); // artist, album, trac, encoding
    //console.info("default file res =",tetra);
    const artist_name = tetra[0];
    const album_name  = tetra[1];
    const track_name = tetra[2];
    const encoding = tetra[3];
    $('#sartselect')[0].value = artist_name;
    init_src_albums( artist_name );
    $('#salbselect')[0].value = album_name;
    init_src_tracks( artist_name, album_name );
    $('#strackselect')[0].value = track_name;
    $('#sencoding')[0].value = encoding;
}


/// Listener for album change; update file select options and encoding
///
function src_album_change(event) {
    if (undefined === EditSource) { return; }
    const album = event.target.value;
    const artist = $('#sartselect').prop('value');
    init_src_tracks( artist, album );
}

/// Reload the current EditSource from the elements in the modal
/// dialog.  This is called when the user clicks the save button.
///
function src_unpack_modal() {
    if (undefined === EditSource) { return; }
    EditSource.type = $('#smedium').prop('value');
    EditSource.url = $('#iurl').prop('value');
    EditSource.artist = $('#sartselect').prop('value');
    EditSource.album = $('#salbselect').prop('value');
    EditSource.file = $('#strackselect').prop('value');
    EditSource.playlist = $('#splist').prop('value');
    EditSource.freq = $('#sfreq').prop('value');
    EditSource.dynamic = $('#sdynamic').prop("checked");
    EditSource.repeat = $('#srepeat').prop("checked");
    EditSource.color = $('#scolor').prop('value');
    EditSource.alternate = $('#salternate').prop('value');
    if (!EditSource.registered) {
        EditSource.name = $('#sname').prop('value');
        //console.info("registering new source: ",EditSource.name);
        addSource(EditSource);
        Sources[EditSource.name] = EditSource;
    }
    const srctab = find_fc_source(EditSource.suid);
    if (srctab != undefined) {
        srctab.style.backgroundColor = EditSource.color;
    }
}


/// Hide elements whose ids are in array hide[], show those in show[].
///
function disp_fields( show, hide )
{
    for (const hid of hide) {
        let helt=document.getElementById(hid);
        helt.style.display='none';
    }
    for (const sid of show) {
        let selt=document.getElementById(sid);
        selt.style.display='inline';
    }
}

/// Configure source dialog fields based on source type smedium.
///
function config_src_fields(smedium) {
    switch (smedium) {
    case 'radio':
        disp_fields(['lfreq'],
                    ['lurl','lart','lalb','lfile','lplist','lrepeat','ldynamic']);
        let sf=document.getElementById("sfreq");
        sf.setAttribute( "value", EditSource.freq );
        break;
    case 'hdradio':
          disp_fields(['lfreq'],
                        ['lurl','lart','lalb','lfile','lplist','lrepeat','ldynamic']);
          sf.setAttribute( "value", EditSource.freq );
          break;
    case 'stream':
        disp_fields(['lurl','ldynamic'],
                    ['lfreq','lart','lalb','lfile','lplist','lrepeat']);
        break;
    case 'file':
        disp_fields(['lfile','lalb','lart','lrepeat','ldynamic'],
                    ['lfreq','lurl','lplist']);
        break;
    case 'directory':
        disp_fields(['lart','lalb','lrepeat','ldynamic'],
                    ['lfreq','lfile','lurl','lplist']);
        break;
    case 'playlist':
        disp_fields(['lplist','lrepeat','ldynamic'],
                    ['lfreq','lart','lalb','lurl','lfile']);
        break;
    default:
        throw new TypeError("Unknown src type: "+smedium);
    }
    document.getElementById("smedium").value = smedium;
}


/// Append the named option to the selector. Note this does not clear the
/// current options, just augments them.
///
function sel_add_option( selector, optname ) {
    const nopt = document.createElement( 'option' );
    const noptext = document.createTextNode( optname );
    nopt.appendChild(noptext);
    selector.appendChild(nopt);
}

/// Remove all the options from a selector.
///
function sel_clear_options( selector ) {
    while (selector.lastElementChild) {
        selector.removeChild(selector.lastElementChild);
    }
}

/// Initialize ARTIST selector with dir options from host_res().
/// This only needs to be done once--on load.
///
function init_src_artists( smodal ) {
    const saselect = smodal.querySelector('#sartselect');
    for (const dname of Object.keys(host_res())) {
        sel_add_option( saselect, dname );
    }
}

/// Initialize ALTERNATE selector with a list of all sources + "OFF"
/// excluding source named by argument banned.
///
function init_src_alts(banned) {
    let salternate = $('#salternate')[0];
    sel_clear_options( salternate );
    sel_add_option( salternate, "OFF" );
    for (const sname of Object.keys(Sources)) {
        if (sname != banned) sel_add_option( salternate, sname );
    }
}

/// Initialize the ALBUM selector with all albums for a given artist.
/// given a directory name and modal dialog.
///
function init_src_albums( artist ) {
    let salbselect = $('#salbselect')[0];
    sel_clear_options( salbselect );
    //console.info("albums for artist ",artist);
    if (artist in host_res()) {
        for (const albname of Object.keys(host_res()[artist])) {
            //console.info(" album: ",albname);
            sel_add_option( salbselect, albname );
        }
    }
    // also init tracks for default album selection
    const album = $('#salbselect').prop('value');
    init_src_tracks(artist,album);
}

/// Initialize the TRACK selector with all files in a directory,
/// given artist and album names. Update encoding.
//
function init_src_tracks( artist, album ) {
    const stselect = $('#strackselect')[0];
    sel_clear_options( stselect );
    if (artist in host_res()) {
        const alb = get_host_album(artist,album);
        if (alb != undefined) {
            for (const tname of alb["tracks"]) {
                sel_add_option( stselect, tname );
            }
            $('#sencoding')[0].value = alb.encoding;
        }
    }
}

/// Initialize playlist selector from host lib
///
function init_src_playlist( smodal ) {
    let splist = smodal.querySelector('#splist');
    const hpl = host_playlists();
    for (const pname of Object.getOwnPropertyNames(hpl)) {
        sel_add_option( splist, pname );
    }
}

/// Delete from the calendar widget all events with title eq to sname.
/// If testp is true, it just counts the matching events.
/// Returns the number of events matched/deleted.
///
function delEventsByTitle(sname,testp=true) {
    const sameName = function(einfo) { return(einfo.title == sname); }
    let matches = TheCalendar.getEvents().filter( sameName );
    const nm = matches.length;
    if (testp) { return nm; }
    if (nm > 0) {
        for (let einfo of matches) {
            einfo.remove();
        }
    }
    return nm;
}

/// Initialize the Source edit dialog. This should be called when the
/// host library has been loaded.
///
function init_modals() {
    const hoptions=false; // TODO: determine listener option support in browser
    let smodal = document.getElementById("srcModal");
    //
    $('#scancel').on('click', function() {
        smodal.style.display = "none";
        EditSource = undefined;
        console.warn("cancel changes to src ");
    });
    $('#ssave').on('click',function() {
        if (validate_src_dialog()) {
            src_unpack_modal();   // save changes
            smodal.style.display = "none";
            markModified();
            EditSource = undefined;
        } // else not valid, don't close
    });
    $('#strash').on('click', function() {
        if (undefined === EditSource) { return; }
        const sname = EditSource.name;
        const nce=delEventsByTitle(sname,true);   // any matching in calendar?
        if (nce > 0)  {
            if (!confirm("Delete "+nce+" matching event(s) from calendar?")) {
                return;         // user changes mind...abort
            }
            delEventsByTitle(sname,false);  // delete matching events
        }
        // $('#outer_'+sname).empty();     // remove button from DOM
        $('#outer_'+sname).remove();     // remove container from DOM
        delete Sources[sname];          // Delete from Sources array
        smodal.style.display = "none";  // hide modal
        markModified();
        EditSource = undefined;

    });
    smodal.querySelector('#smedium').addEventListener("change",
                                                    src_medium_change,hoptions);
    smodal.querySelector('#scolor').addEventListener("change",
                                                     src_col_change, hoptions);
    smodal.querySelector('#sartselect').addEventListener("change",
                                                        src_artist_change,hoptions);
    smodal.querySelector('#salbselect').addEventListener("change",
                                                        src_album_change,hoptions);
    // file widget options will be added dynamically
    let lfile = smodal.querySelector('#lfile');
    lfile.style.display = "none";
    init_src_artists( smodal );
    init_src_playlist( smodal );
    // When the user clicks anywhere outside of a modal, close it per cancel.
    window.onclick = function(event) {
        if (event.target == evtModal) {
            evtModal.style.display = "none";
        }
        if (event.target == srcModal) {
            srcModal.style.display = "none";
            EditSource = undefined;
        } }
}

/// Assign actions to the Source pane buttons: Check, New
///
function init_source_pane() {
    $('#check-sources').on('click',function(){ validate_sources() });
    $('#new-audio-source').on('click',function(){ make_new_source() });
}

// Edit the srcobject in the given dialog: load fields and display If
// the srcobj is unregistered, the user is allowed to edit the name;
// otherwise the name appears as just a large label in the header.
//
function editSource( srcobj )
{
    EditSource = srcobj;
    const dialog=document.getElementById('srcModal');
    const sn_input=dialog.querySelector('#sname');
    if (srcobj.registered) {
        $('#sm_editlabel').hide();
        $('#strash').show();
        $('#sm_staticname').html(srcobj.name).show();
        dialog.querySelector('#smedium').disabled=true;
        dialog.querySelector('#sfreq').disabled=true;
        dialog.querySelector('#sartselect').disabled=true;
        dialog.querySelector('#salbselect').disabled=true;
        dialog.querySelector('#strackselect').disabled=true;

    } else {
        // unregistered: allow user to edit name (initially selected)
        $('#sm_staticname').hide();
        $('#strash').hide();
        $('#sm_editlabel').show()
        sn_input.value=srcobj.name;
        dialog.querySelector('#smedium').disabled=false;
        dialog.querySelector('#sfreq').disabled=false;
        dialog.querySelector('#sartselect').disabled=false;
        dialog.querySelector('#salbselect').disabled=false;
        dialog.querySelector('#strackselect').disabled=false;
    }
    init_src_alts(srcobj.name); // don't include this source as an alternate
    config_src_fields(srcobj.type);
    dialog.querySelector("#sheader").style.backgroundColor = srcobj.color;
    dialog.querySelector('#scolor').value = srcobj.color;
    dialog.querySelector('#srepeat').checked = srcobj.repeat;
    dialog.querySelector('#sdynamic').checked = srcobj.dynamic;
    dialog.querySelector('#sfreq').value = srcobj.freq;
    dialog.querySelector('#sencoding').value = srcobj.encoding;
    dialog.querySelector('#sencoding').disabled=true; // always?
    //.........
    if (undefined != srcobj.artist) {
        dialog.querySelector('#sartselect').value = srcobj.artist;
        init_src_albums( srcobj.artist );
        dialog.querySelector('#salbselect').value = srcobj.album;
        init_src_tracks(srcobj.artist, srcobj.album);
        dialog.querySelector('#strackselect').value = srcobj.file;
    } else {
        config_new_file_res();
    }
    //.........
    if (isFinite(srcobj.duration)) {
        $('#smDuration').text("Duration: "+timeIntStr( srcobj.duration ));
    } else {
        $('#smDuration').text("Duration: indefinite");
    }
    //.........
    dialog.querySelector('#splist').value = srcobj.playlist;
    dialog.querySelector('#iurl').value = srcobj.url;
    dialog.style.display = "block"; // display as modal
    dialog.querySelector("#salternate").value = srcobj.alternate;
    if (! srcobj.registered) {
        sn_input.select();
        sn_input.focus();
    }
}

/// Add src to Sources and add a button for it that will invoke
/// the edit modal dialog named. The source is marked as 'registered'.
///
function addSource( src ) {
    const xclasses='fc-event fc-h-event fc-daygrid-event fc-daygrid-block-event fc-event-draggable'
    const outer=$("<div></div>",
                  {id: "outer_"+src.name,
                   class: xclasses});
    let inner=$('<div></div>',
                {id: src.suid,
                 class: 'fc-event-main',
                 click: function(evt) { editSource( src );
                                        return false;}}).text(src.name);
    inner[0].style.backgroundColor = src.color;
    src.registered = true;
    outer.append(inner);
    outer.appendTo('#external-events-list');
    //
}

/// Validate all sources, displaying the result and returning true if all okay.
///
function validate_sources() {
    const n = String(Object.keys(Sources).length);
    //
    // TODO: make it really validate the sources.
    //
    // alert(n+" of "+n+" sources are well formed.");
    return true;
}


/// User clicks 'new source'. Create a new source with an editable name.
/// It will be registered if the edit modal is successfully saved.
///
function make_new_source() {
    //console.info("provisionally create new audio source");
    editSource( new RcalSource(DefaultSrcName,'radio','wfm',99.9) );
}

/// Add a source named sname given rsked schedule definition object
/// sdef.  Sources or announcements end up in lists Sources and
/// Announcements respectively. Those with names starting'%' are imported but
/// not displayed.
///
function import_source(sname, sdef) {
    const medium = sdef['medium'];
    const encoding = sdef['encoding'];
    const location = sdef['location'];
    var src;
    switch( medium ) {
    case 'radio':
    case 'playlist':
    case 'stream':
    case 'directory':
    case 'file':
        src = new RcalSource(sname,medium,encoding,location);
        break;
    default:
        throw new Error("Unknown source medium on import: "+medium);
    }
    if ('alternate' in sdef) {
        src.alternate = sdef['alternate'];
    }
    if ('dynamic' in sdef) {
        src.dynamic = sdef['dynamic'];
    }
    if ('repeat' in sdef) {
        src.repeat = sdef['repeat'];
    }
    if ('announcement' in sdef) {
        src.announcement = sdef['announcement'];
        src.ann_location = sdef['location'];
    }
    if ('text' in sdef) {
        src.text = sdef['text'];
    }
    if ('duration' in sdef) {
        src.duration = sdef['duration'];
    }
    src.registered = true;
    if (src.announcement) {
        Announcements[sname] = src;
        src.color = AnnColor;
        //console.info("import announcement ", src.name,src.suid,src.text);
        if ("%"!=sname.slice(0,1)) {
            addAnnounce( src );
        }
    } else {
        Sources[src.name] = src;
        //console.info("import source ",sname,src.suid);
        if ("%"!=sname.slice(0,1)) {
            addSource( src );
        }
    }
}

/// Import sources from LoadedSchedule including programs and announcements.
/// Any existing Sources and Announcements are discarded.
///
function import_rsked_sources() {
    const srcs = LoadedSchedule['sources'];
    const src_keys = Object.keys(srcs);
    Sources = {};
    Announcements = {};
    src_keys.forEach( (sname,j) => { import_source( sname, srcs[sname] ); });
}


/// Return a Date object for canonical day /i/ (0=Sun) given time
/// string /tstr/ e.g. "08:30:00"
///
function canon_date(i,tstr) {
    if (! Number.isInteger(i) || i < 0 || i > 6) {
        console.error("canon_date():: bad day of week: ",i);
        return null;
    }
    let j=0, ta= [0,0,0]; // h, m, s
    for ( let te of tstr.split(':') ) {
        const i = parseInt(te);
        if (NaN === i) {
            console.error("canon_date():: bad time ",tstr);
            break;
        }
        ta[j++] = i;
    }
    return new Date(gYear0, gMonth0, gDay0+i, ta[0], ta[1], ta[2]);
}


/// Return a new Date by adding a small amount of time to start time
/// for announcement /d1/ to make it large enough to show up on the
/// calendar.  Currently: 20 minutes.
///
function extend_announcement( d1 ) {
    let d2 = new Date(d1);
    // TODO: avoid letting announcements creep over midnight boundary.
    d2.setMinutes( d1.getMinutes()+20 );
    return d2;
}



/// Add events from LoadedSchedule to calendar.
/// - Ignore "OFF" events--these are implicit.
/// - Handle announcemens specially
///
function import_rsked_events() {
    const dayprograms = LoadedSchedule['dayprograms'];
    // cycle through days of the week, sunday...saturday:
    for (let i=0; i<7; i++) {
        let day = DayNames[i];
        let dslots = dayprograms[day];
        let t_start = null;
        let c_prog = null;
        //console.info(day," has ",dslots.length," slots");
        for (let slot of dslots) {
            if (slot.hasOwnProperty("program")) {
                let prog = slot.program;
                let t_slot = slot.start;
                if ((t_start !== null) && (c_prog !== "OFF")) {
                    const c_src = Sources[c_prog];
                    if (undefined === c_src) {
                        console.error("skipping undefined source: ", c_prog);
                    } else {
                        let dstart = canon_date(i,t_start);
                        let dslot  = canon_date(i,t_slot);
                        //console.info(day," program ",t_start,"->",t_slot," : ",c_prog);
                        let evt = {start: dstart, end: dslot, title: c_prog,
                                   color: c_src.color };
                        TheCalendar.addEvent(evt);
                    }
                }
                c_prog = prog;
                t_start = t_slot;
            }
            // its an announcement and thus does not affect regular schedule
            else if (slot.hasOwnProperty("announce")) {
                let ann = slot.announce;
                let t_ann = slot.start;
                const dstart = canon_date(i,t_ann);
                const dend = extend_announcement( dstart );
                const c_ann = Announcements[ann];
                if (undefined === c_ann) {
                    console.error("skipping undefined announcement: ", ann);
                } else {
                    //console.info(day," announcement ",t_ann," : ",ann);
                    let evt = {start: dstart, end: dend,
                               title: ann, color: c_ann.color };
                    TheCalendar.addEvent(evt);
                }
            }
        }
    }
}

///  A N N O U N C E M E N T S

/// Return true if the argument names an announcment (not a program)
/// Criterion: in the global object Announcements{}
///
function is_announcement(src_name) {
    return Announcements.hasOwnProperty(src_name);
}


/// Add announcement to announcement list as an fc event
///
function addAnnounce( ann ) {
    //console.info("add announcement ",ann.name, ann.suid, ann.color);
    const xclasses='fc-event fc-h-event fc-daygrid-event fc-daygrid-block-event fc-event-draggable'
    const outer=$("<div></div>",
                  {id: "outer_"+ann.name,
                   class: xclasses});
    let inner=$('<div></div>',
                {id: ann.suid,
                 class: 'fc-event-main',
               }).text(ann.name);
    inner[0].style.backgroundColor = ann.color;
    outer.append(inner);
    outer.appendTo('#external-announce-list');
    //
}

///////////////////////////////////////////////////////////////////////////////
///                               CALENDAR
/// (relies on fullcalendar)
///

/// Initialise and render the calendar.

var Event = FullCalendar.event;
// Allow dragging from external event source lists
var Draggable = FullCalendar.Draggable;
var containerEl = document.getElementById('external-events');

/// DEBUG-only:  list the calendar events on the console
/// TODO: disable in release
function dumpcal() {
    let j=1;
    for (let jev of TheCalendar.getEvents()) { 
        console.info(j++,'. ',jev.title,jev.start.toString());
    }
}

/// Mark the schedule (including sources) as modified and requiring Save.
///
function markModified() {
    ScheduleModified = true;
    updateCalTitle();
}

/// Force the calendar to show the new title.
function updateCalTitle() {
    TheCalendar.setOption('titleFormat',
                          function(date) {
                              return("v." + SavedSchedule.version 
                                     + (ScheduleModified ? " - Modified" : '')); });
}

/// Used on events whose sources have a known duration, it will adjust
/// the end time of the event to be the start time + the duration.
/// We insist on at least 1 second of gap
/// Invoked via callback when, for example, recording sources are added.
///
function maybeResize(evt) {
    const src = Sources[evt.title];
    if (undefined === src) { return; }
    const dur = src.duration;
    if (undefined === dur) { return; } // no defined duration--don't touch
    const new_end = dateAddSecs(evt.start, Math.floor(dur+1));
    //console.info("Adjust ",evt.title," @ ",evt.start," to ",dur," seconds");
    evt.setEnd(new_end);
}


document.addEventListener('DOMContentLoaded', function() {
        var calendarEl = document.getElementById('calendar-widget');
        var calendar = new FullCalendar.Calendar(calendarEl, {
            initialView: 'timeGridWeek',
            initialDate: gDay0Str,
            // FC requires an initial date value, otherwise it defaults
            // to the current day, which we don't want (adds some
            // weird 'today' formatting).
            //
            // eventShortHeight: 30,
            eventOverlap: true,
            titleFormat: function(date) {
                return ('RCAL scheduler for rsked radio');
            }, // Note: FC's built-in title format only displays dates.
          customButtons: {
            saveButton: {
              text: 'Save',
              click: function() {
                  console.group("Save schedule");
                  let newsked = export_to_rsked();
                  // Ajax post schedule under key 'schedule'.
                  // Callback just notifies user of acceptance.
                  let d = new Date();            // now
                  newsked.version = d.toISOString();
                  SavedSchedule = newsked;
                  //console.info("The current schedule version is", newsked.version)
                  // Ajax post of schedule under key 'schedule'. Callback notifies user.
                  $.post( "newsked.php",  { schedule : JSON.stringify(newsked) },
                          function (txt) {
                              console.groupEnd();
                              alert('Saved schedule: '+txt);
                              // Note well: LoadedSchedule != newsked
                              ScheduleModified = false;
                              updateCalTitle();
                          },
                        "text");              }
            },
            clearButton: {
                text: 'clear',
                click: function() {
                  const EvList = calendar.getEvents(Event);
                  var r = confirm('Are you sure you want to clear the schedule?');
                  if (r == true) {  // Removes all events from calendar;
                    EvList.forEach((Event) => Event.remove());
                  }

               }
            },
            loadButton: {
              text: 'revert',
              click: function() {
                load_schedule( gScheduleResource );
              }
            },
            compactButton: {
              text: 'compact',
              click: function() { compactSchedule(); }
              }
            }, // ^customButtons^
          headerToolbar: {
              right: 'loadButton clearButton',
              center: 'title',
              left: 'saveButton compactButton'
          },
          allDaySlot: false, //Disallow all-day events
          dayHeaderFormat: { weekday: 'short' }, //Display only day of the week, no date
          slotDuration: '00:30:00', // Add time slots every thirty minutes
          slotLabelInterval: '01:00:00', // Display text labels every hour
          displayEventTime : false,
          droppable: true, // Bool - User can drag and drop events
          editable: true, // Bool - User can edit calendar
          events: [
            { // this object will be "parsed" into an Event Object
              title: 'Title', // Title property
              description: 'Description', // Description property
              start: gDay0Str+'T09:00:00', // Start time property
              end:   gDay0Str+'T10:00:00', // End time property
              // daysOfWeek: [], // Array of days on which event occurs. Sunday = 0
              forceEventDuration: true,
              overlap: false, // Events are by default not allowed to overlap
              classNames: '', // Event classes
              backgroundColor: 'black',  //Event colour
              extendedProps: {
                medium: 'radio'
              },
            }
          ],
          eventReceive: function(info) {
              const evt = info.event;
              markModified();
              maybeResize( evt );
          },
          eventClick: function(info) {
              // In the event the URL property has been assigned, do NOT navigate to it!
              info.jsEvent.preventDefault();
              // nb. event may have just been deleted..
              // console.info("Event info:",info.event.title, info.el.style.backgroundColor,
              //               info.end);
            call_evt_modal(info); // Open the event edit modal
          }
        });
        // -----------------------------------------------------------------
        // Initialise the external events
        new Draggable(containerEl, {
            itemSelector: '.fc-event-main',
            eventData: function(eventEl) {
                let src_name = eventEl.innerText;
                let src_col = eventEl.style.backgroundColor;
                let is_ann = is_announcement(src_name);
                return {
                    title: src_name,
                    overlap: true,    // is_ann, -- hmm then you cannot cross anns
                    // unfortunately, 'list-item' not supported in timeGridweek view
                    display: (is_ann ? 'list-item' : 'auto'),
                    duration: (is_ann ? '00:30' : '01:00'),
                    displayEventTime : false,
                    backgroundColor: src_col,
                    borderColor: src_col,
                    description: 'program'
                };
            },
        });

      TheCalendar = calendar;  // Set the global calendar reference:
      calendar.render();
    });

/// Order two rsked (or FullCalendar) events for sorting purposes.
function start_order(e1, e2) {
    let s1 = e1.start, s2 = e2.start;
    if (s1 < s2) { return -1 };
    if (s1 > s2) { return 1 };
    return 0;
}


/// Make an array of rsked events satisfy these constraints
/// - events must be ordered by start time
/// - if the first event on a day does not begin at 00:00 add
///   an OFF event starting at 00:00
/// - if the final event of the day does not end at 24:00 add
///   a terminal OFF event
/// - any empty days need a dummy OFF event
/// Arg darray (call by reference) is modified in place.
///
function normalize_day( darray, dayname ) {
    if (0 == darray.length) { // empty: an entirely quiet day
        darray.push( {start : "00:00:00", program : "OFF" } );
        return;
    }
    darray.sort( start_order );
    if (darray[0].start != "00:00:00") { // start at OFF if needed
        darray.splice(0,0,{start : "00:00:00", finish: darray[0].start,
                           program : "OFF" });
    }
    // Fill any gaps between events end and next event start with OFF
    let fin = darray[0].finish; // last extended event's finish time
    let j=1;
    while (j < darray.length) {
        let strt = darray[j].start;
        if (darray[j].hasOwnProperty("program")) { // not an announcement
            if (strt != fin) {
                //console.info(dayname," ",j," ",strt," fill program gap, last fin=",fin)
                darray.splice( j, 0, {start : fin, finish : strt, program : "OFF" } );
            }
            fin = darray[j].finish;
        }
        // else {
        //  console.info(dayname," ",j," ",strt," announcement: ",
        //               darray[j].announce);
        // }
        j++;
    }
    if (darray[j-1].finish != "00:00:00") { // maybe pad to end of day
        darray.splice( j, 0, {start : fin, finish : "00:00:00", program : "OFF" });
    }
    // Remove helper properties (like 'finish') not part of rsked schema
    for (let d of darray ) {
        delete d['finish'];
    }
}

/// Return a dictionary of arrays indexed by DayNames containing the events
/// Each array element is an object with fields start, end, announce/program
/// as needed for writing schedules.
///
function extract_dayprograms() {
    var dpobj = {};                       // each gets an array in dpobj
    for (let d of DayNames) {
        dpobj[d] = Array();
    }
    // Collect events from TheCalendar.
    // Note: assume events are minimally consistent with rsked constraints
    //    e.g. do not span days, are not marked "allDay", ...
    // Aside: getEvents returns events in no particular order
    for (let evt of TheCalendar.getEvents()) {
        let ts = evt.start;
        let te = evt.end;  // null if not set, e.g. left to default 1hr duration
        if (null == te) {
            te = new Date(ts);
            te.setHours( 1 + ts.getHours() );
        }
        let dow = DayNames[ts.getDay()]; // Sunday==0, Monday==1, ...
        let tstr = ts.toLocaleTimeString('en-GB');
        let zstr = te.toLocaleTimeString('en-GB');
        if (is_announcement(evt.title)) {
            dpobj[dow].push({"start" : tstr, "announce" : evt.title, finish : tstr});
        } else {
            dpobj[dow].push({"start" : tstr, "program" : evt.title, finish : zstr});
        }
        //console.info(evt.title," on day",dow,"at",tstr,"\n");
    }
    return dpobj;
}


/// Contruct the dayprogram object: each element is an array
/// associated with a day of the week.
///
function export_dayprograms() {
    var dpobj = extract_dayprograms();
    for (let dow of DayNames) {     // normalize each day:
        normalize_day( dpobj[dow], dow );
    }
    return dpobj;
}


// cms  on day monday at 07:30:00 
// dnow  on day monday at 12:00:00 
// cms  on day monday at 13:00:00 
// nis  on day monday at 14:00:00 
// motd-ymd  on day monday at 15:30:00 
// ksjn  on day monday at 15:00:00


/// Return the effective rsked location field for a source s.
/// If an announcement, return the original location field
/// If a radio source, return freq.
/// If a file/directory source, return file/directory
/// If a playlist source, return playlist
/// If a stream, return the url
///
function src_location( s ) {
    if (s.announcement) {
        return s.ann_location;
    }
    switch (s.type) {
    case 'directory':
        return (s.artist + '/' + s.album);
    case 'file':
        return (s.artist + '/' + s.album + '/' + s.file);
    case 'radio':
        return s.freq;
    case 'playlist':
        return s.playlist;
    case 'stream':
        return s.url;
    default:
        return null;
    }
}


/// Export an RcalSource object x as a plain source object for rsked.
///
function src_export(x) {
    let p = { encoding: x.encoding,
              medium: x.type,
              location: src_location(x),
              repeat: x.repeat,
              dynamic: x.dynamic,
              alternate: x.alternate };
    if (x.hasOwnProperty('announcement')) {
        p.announcement = x.announcement;
    }
    if (x.hasOwnProperty('duration')) {
        p.duration = x.duration;
    }
    if (x.hasOwnProperty('text')) {
        p.text = x.text;
    }
    return p
}

/// Return an rsked-style object representation for all defined
/// sources including hidden announcements and sources not currently
/// used in the schedule.
///
function export_sources() {
    var usrcs = {};
    const src_keys = Object.keys(Sources).sort();
    src_keys.forEach( (skey,i) => {
        usrcs[skey] = src_export( Sources[skey] );
    })
    const ann_keys = Object.keys(Announcements).sort();
    ann_keys.forEach( (akey,j) => {
        usrcs[akey] = src_export( Announcements[akey] );
    });
    return usrcs;
}


/// Convert Calendar events to rsked events, returning the rsked object.
///
function export_to_rsked() {
    //let sked= LoadedSchedule;
    var calendarEl = document.getElementById('calendar-widget');
    var dpgms = export_dayprograms();
    var rsrcs = export_sources();
    var d = new Date();            // now
    var sked = { "encoding" : "UTF-8",
                 "schema" : "2.0",
                 "version" : d.toISOString(),
                 "library" : LoadedSchedule.library,
                 "playlists" :  LoadedSchedule.playlists,
                 "sources" : rsrcs,
                 "dayprograms" : dpgms
               };
    return sked;
}


/// Call this prior to importing a new schedule.
/// - Empty the Sources array
/// - Empty the Announcements array
/// - Remove dom elements from external-events-list
/// - Remove dom elements from external-announce-list
/// - Remove all events from the FC calendar widget
///
function flush_schedule() {
    Sources.length = 0;
    Announcements.length = 0;
    $('#external-events-list').empty(); // remove all child nodes (<div>)
    $('#external-announce-list').empty();
    const EvList = TheCalendar.getEvents(Event);
    EvList.forEach((Event) => Event.remove());
}


/// Fetch the schedule from url src. It will be globally available in
/// LoadedSchedule.  Callback will trigger a 'scheduleLoaded' custom
/// event (see handler below).
///
function load_schedule( src ) {
    console.group("Load Schedule");
    $.getJSON( src, function(data) {
        LoadedSchedule = data;
        $(document).trigger('scheduleLoaded'); });
}

/// Display the schedule just loaded into global LoadedSchedule.
///
$(document).on('scheduleLoaded',
               function() {
                   if ("object" === typeof(LoadedSchedule)) {
                       flush_schedule();  // erase old schedule rendering
                       import_rsked_sources();
                       import_rsked_events();
                       SavedSchedule = LoadedSchedule;
                       ScheduleModified = false;
                       updateCalTitle();
                   } else {
                       alert('failed to load current schedule.');
                   }
                   console.groupEnd();
               });


/// Find all events that have the same title, start time and end time.
///
function peerEvents(evt) {
    const etitle = evt.title;
    const tstart = evt.start.toLocaleTimeString();
    const tfinal = evt.end.toLocaleTimeString();
    // collect events that match
    return TheCalendar.getEvents().filter(e => (e.title == etitle) &&
                                          (e.end.toLocaleTimeString() == tfinal) &&
                                          (e.start.toLocaleTimeString()==tstart));
}

/// Return an object with attributes corresponding to days of the week
/// and each value an array of ordered /program/ calendar events (no
/// announcements).  This structure is used for *compaction* of programs.
///
function extract_pevents() {
    var dpobj = {};                       // each gets an array in dpobj
    for (let d of DayNames) {
        dpobj[d] = Array();
    }
    // Collect events from TheCalendar.
    for (let evt of TheCalendar.getEvents()) {
        let ts = evt.start;
        let te = evt.end;  // null if not set, e.g. left to default 1hr duration
        if (null == te) {  // this should really not occur...
            te = new Date(ts);
            te.setHours( 1 + ts.getHours() );
        }
        let dow = DayNames[ts.getDay()]; // Sunday==0, Monday==1, ...
        let tstr = ts.toLocaleTimeString('en-GB');
        let zstr = te.toLocaleTimeString('en-GB');
        if (! is_announcement(evt.title)) {  // programs only
            dpobj[dow].push(evt);
        }
    }
    return dpobj;
}


/// analyze the events on a given day and autofix any that overlap
/// darray is an (unsorted) array of FullCalendar events for the given day
/// corresponding to programs (no announcements).
///
/// Compaction Policy:
/// 0.  Events with no overlap are untouched
/// 1.  If an overlapping event has a natural duration, shrink it to that
///     duration
/// 2.  If overlap is properly within the earlier, split the earlier event
///     to a before and after pair; if the later event's duration goes beyond
///     the end of the earlier one, just foreshorten the earlier one to abut.
///
function compact_day( darray, dow ) {
    // console.info("; compacting ",dow );
    let elast = undefined;  // previous event
    let tfin = 0;  // end time (seconds into day) of previous event
    let j=0;
    darray.sort( start_order );
    while (j < darray.length) {
        let ej = darray[j];
        // TODO: maybe shrink
        let tstrt = getDaySecsDate( ej.start );
        if (tstrt < tfin) {
            console.warn(";  ", tstrt," : ",ej.title," overlaps ",elast.title);
            // fix it per policy
            let tend = getDaySecsDate( ej.end );
            if ( tfin <= tend ) {
                elast.setEnd( ej.start );
                //console.info(";   foreshorten ", elast.title);
            } else {
                //console.info(";   split ", elast.title);
                const enew =  { start:  ej.end, end: elast.end, overlap: true,
                                color: elast.backgroundColor,  title: elast.title,  };
                let ep = TheCalendar.addEvent( enew );
                elast.setEnd( ej.start );
                darray.splice( j+1, 0, ep );
            }
        }
        // else {
        //     console.info(";  ",tstrt," : ",ej.title);
        // }
        tfin = getDaySecsDate( ej.end );
        elast = ej;
        j++;
    }
}



/// Find and fix overlaps of program events
///
function compactSchedule() {
    var dpobj = extract_pevents();
    for (let dow of DayNames) {     // compact each day of the week
        compact_day( dpobj[dow], dow );
    }
}

/////////////////////////////////////////////////////////////////////////////////
//// EVENT Edit Dialog

/// When the user clicks on an event in the calendar, raise this
/// dialog that allows some tweeking, e.g. repeat pattern days of week.
/// @arg calEvent  eventClickInfo object, {event,el,jsEvent,view}
///
function call_evt_modal(calEvent) {
      const evt = calEvent.event;
      let emodal = document.getElementById("evtModal");
      let mdheader = emodal.childNodes[1].childNodes[1];
      mdheader.style.backgroundColor = calEvent.el.style.backgroundColor;
      let etitle = document.getElementById("evtTitle");
      etitle.innerText = evt.title;
      const src = Sources[evt.title]; // lookup the source
      let edesc = document.getElementById("evtDesc");
      const estart = evt.start; // a property (a Date object)
      const eDay = estart.getDay(); // 0=Sun, ..., 6=Sat
      const efinal = evt.end;
      if (undefined===calEvent.event.description) {
        // description will show timings unless otherwise defined as some string
        const diffSec = (efinal - estart)/1000; // cvt from milliseconds
        let dtxt = estart.toLocaleTimeString()+' to '+efinal.toLocaleTimeString()
            + ": "+timeIntStr(diffSec);
        if (undefined != src) {
            const sdur = src.duration;
            if (isFinite(sdur)) {
                dtxt += (" (Src dur.: "+timeIntStr(sdur)+")")
            }
        }
        edesc.innerText = dtxt;
      } else {
          edesc.innerText = calEvent.event.description;
      }
      // Set up check boxes. Rather than yoking events into a group,
      // we treat all other events with same title and times as a group.
      const cbs  = $('#eCbSet > label > input');
      cbs.prop('checked',false);
      cbs.prop('disabled',false);
      const peersByDay = new Array(7); // [0..6] initially undefined
      for (const ep of peerEvents(evt)) {
          const u = ep.start.getDay();
          peersByDay[ u ] = ep; // memo existing peer, possibly self
          cbs[ u ].checked = true;
      }
      // The checkbox for the day clicked on is disabled hence locked ON
      cbs[eDay].disabled = true;

      // TODO: determine type of source and adjust the modal accordingly(?)
      emodal.style.display = "block";

      // When the user clicks on the close element (X), just close the dialog
      let eclose = document.getElementById("eclose");
      eclose.onclick = function() {
          emodal.style.display = "none";
      }
      // When user clicks save element, apply changes and close dialog
      let esave = document.getElementById("esave");
      esave.onclick = function() {
          // react to any implicit changes in repeat pattern
          for (let i=0; i<7; i++) {
              if (cbs[i].checked && peersByDay[i]===undefined) {
                  const neuStart = new Date(gYear0, gMonth0, gDay0+i, estart.getHours(),
                                            estart.getMinutes(), estart.getSeconds());
                  const neuEnd= new Date(gYear0, gMonth0, gDay0+i, efinal.getHours(),
                                         efinal.getMinutes(), efinal.getSeconds());
                  //console.info("Add new peer for day ",i," ",neuStart,"->",neuEnd);
                  // new clone for day=i
                  TheCalendar.addEvent(
                      { title: evt.title,
                        start: neuStart,
                        end: neuEnd,
                        backgroundColor: evt.backgroundColor,
                        borderColor: evt.borderColor } );
              }
              else if (peersByDay[i]!=undefined && ! cbs[i].checked) {
                  //console.info("Delete peer on day ",i);
                  peersByDay[i].remove(); // removes this peer
              }
          }
          emodal.style.display = "none";
      }
      // When the user clicks on the delete button, confirm and then remove the
      // event from the calendar, close the dialog and refresh calendar.
      let edel = document.getElementById("edelete");
      edel.onclick = function() {
          if (confirm('Delete this "'+etitle.innerText+'" event?')) {
              emodal.style.display = "none";
              calEvent.event.remove(); // removes this one event
              TheCalendar.render();
          }
      }
      return emodal;
}

/////////////////////////////////////////////////////////////////////////////

/// Initialize GUI and try to load a schedule.

load_host_library( gHostLibraryResource );
