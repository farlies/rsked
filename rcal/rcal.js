//////////////////////////////////////////////////////////////////////////////
//// Rcal Calendar Editor

"use strict";

/// G L O B A L S

/// The source being edited, if any; otherwise undefined.
var EditSource=undefined;

/// Global object used as map of sources by their (unique) names,
///    "cms"  => <RcalSource cms>
///
var Sources = {};
var Announcements = {};

const DefaultSrcName='(new name)';

///////////////////////////////////////////////////////////////////////////////

/// These are defined in 'testdata.js' during early development:
//
// var gFiles = { }
// var gPlaylists = [ ]
// var rsked_schedule = { }

/// Retrieve the playlists (an ARRAY) available on the host.
/// (This is a stub for now.)
///
function host_playlists() {
    return gPlaylists;
}

function host_announcements() {
    return gAnnouncements;
}

/// Retrieve the recorded audio resources on the host (nested Objects).
/// (This is a stub for now.)
///
function host_res() {
    return gFiles;
}

/// Return the album resource for a given artist.
/// It will return undefined if there is no such artist/album.
///
function get_host_album(artist,album) {
    const res=host_res();
    return res[artist][album];
}


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
    alternate;   // string name of alt source
    encoding;    // 'ogg','mp3','mp4','wfm','mixed'
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
    constructor(sname,medium,encoding,sres,repeat=true,dynamic=false) {
        var pfile;
        var parts;
        this.name = sname;
        this.registered = false;
        this.suid = uuidv4();
        this.color = medium_to_color(medium);
        this.type = medium;
        this.encoding = encoding;
        this.repeat = repeat;
        this.dynamic = dynamic;
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
        console.log("Cannot find an fc-event-main div for ",suid);
        return undefined;
    }
}

// listener: respond to change of Source medium, e.g. radio -> stream
// by updating the modal dialog fields' visibility, showing only the
// relevant fields for the type.
//
function src_medium_change(event) {
    if (undefined === EditSource) { return; }
    let neumedium = event.target.value;
    //console.log('src_medium_change:', nemedium); // DEBUG only
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

/// Are form contents consistent enough to allow it to be saved?
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
    // ... other checks
    return true;
}


// listener for artist change; update album select options
function src_artist_change(event) {
    if (undefined === EditSource) { return; }
    const neuart = event.target.value;
    init_src_albums( neuart );
}

// listener for album change; update file select options
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
        console.log("registering new source: ",EditSource.name);
        addSource(EditSource);
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
    console.log("albums for artist ",artist);
    if (artist in host_res()) {
        for (const albname of Object.keys(host_res()[artist])) {
            console.log(" album: ",albname);
            sel_add_option( salbselect, albname );
        }
    }
    // also init tracks for default album selection
    const album = $('#salbselect').prop('value');
    init_src_tracks(artist,album);
}

/// Initialize the TRACK selector with all files in a directory,
/// given artist and album names.
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
        }
    }
}


/// Initialize playlist selector from gPlaylists global.
function init_src_playlist( smodal ) {
    let splist = smodal.querySelector('#splist');
    for (const pname of host_playlists()) {
        sel_add_option( splist, pname );
    }
}

// Initialize the Source edit dialog
function init_modals() {
    const hoptions=false; // TODO: determine listener option support in browser
    let smodal = document.getElementById("srcModal");
    //
    $('#scancel').on('click', function() {
        // TODO: cancel changes
        smodal.style.display = "none";
        EditSource = undefined;
        console.log("cancel changes to src ");
    });
    $('#ssave').on('click',function() {
        if (validate_src_dialog()) {
            src_unpack_modal();   // save changes
            smodal.style.display = "none";
            EditSource = undefined;
        } // else not valid, don't close
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

// source pane buttons
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
        $('#sm_staticname').html(srcobj.name).show();
    } else {
        // unregistered: allow user to edit name (initially selected)
        $('#sm_staticname').hide();
        $('#sm_editlabel').show()
        sn_input.value=srcobj.name;
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
    dialog.querySelector('#sartselect').value = srcobj.artist;
    init_src_albums( srcobj.artist );
    dialog.querySelector('#salbselect').value = srcobj.album;
    init_src_tracks(srcobj.artist, srcobj.album);
    dialog.querySelector('#strackselect').value = srcobj.file;
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
/// the edit modal dialog named.
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
    outer.append(inner);
    outer.appendTo('#external-events-list');
    //
    src.registered = true;
    Sources[src.name] = src;
}

/// Validate all sources, displaying the result and returning true if all okay.
///
function validate_sources() {
    const n = String(Object.keys(Sources).length);
    //
    // TODO: make it really validate the sources.
    //
    alert(n+" of "+n+" sources are well formed.");
    return true;
}


/// User clicks 'new source'. Create a new source with an editable name.
/// It will be registered if the edit modal is successfully saved.
///
function make_new_source() {
    console.log("provisionally create new audio source");
    editSource( new RcalSource(DefaultSrcName,'radio','wfm',99.9) );
}

/// These source objects are for small-scale testing of dialog layout only.
/// See also: testdata.js.
///
/*
function load_test_sources() {// name medium  enc. location
    addSource( new RcalSource("KNOW",'radio','wfm', 91.1) );
    addSource( new RcalSource("KSJN",'radio','wfm', 99.5) );
    addSource( new RcalSource('cms','stream','mp3',
                              "http://cms.stream.publicradio.org/cms.mp3"));
    addSource( new RcalSource('Aja','playlist','ogg',"Steely Dan - Aja.ogg.m3u"));

    addSource( new RcalSource('Demons & Wizards','directory','ogg',
                              "Uriah Heep/Demons and Wizards"));

    addSource( new RcalSource("Easy Livin",'file','ogg',
                              "Uriah Heep/Demons and Wizards/03-Easy Livin.ogg",
                              true, false));
}*/

/// Add a source named sname given rsked schedule definition sdef.
///
function import_source(sname, sdef) {
    const medium = sdef['medium'];
    const encoding = sdef['encoding'];
    const location = sdef['location'];
    var src;
    console.log("import source ",sname);
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
    // TODO: capture the 'alternate' source field too
    if ('dynamic' in sdef) {
        src.dynamic = sdef['dynamic'];
    }
    if ('repeat' in sdef) {
        src.repeat = sdef['repeat'];
    }
    addSource(src);
}

/// Add sources from loaded var rsked_schedule
///
function parse_rsked_sources() {
    const srcs = rsked_schedule['sources'];
    for (var sname of Object.keys(srcs)) {
        // we do not display built-in sources: these are added on export
        if ("%"!=sname.slice(0,1)) {
            import_source( sname, srcs[sname] );
        }
    }
}

/// Add default announcements to the announcement list
// Model for announcements

class RcalAnnounce {
    name;        // string name, unique in schedule
    registered;   // bool
    suid;        // string uuid--unique in session; not in schedule
    text;        // text form of announcement e.g. "good morning"
    color;        // dom color spec.
    //
    constructor(sname,suid,stext) {
        var pfile;
        var parts;
        this.name = sname;
        this.registered = false;
        this.suid = uuidv4();
        this.color = "#ffcc66";
        this.text = stext;
    }
};

var gAnnouncements = {
    "motd-ymd" : {"text" : "message of the day - ymd"},
    "motd-md" : {"text" : "message of the day - md"},
};

// for every announcement in gAnnouncements, run 'import_announcement' FIX THIS!

function get_announce() {
  const srcs = gAnnouncements;
  for (var ann of Object.keys(srcs)) {
    import_announcement( ann, srcs[ann] );
    }
}

function import_announcement(sname, sdef) {
    const suid = uuidv4();
    const stext = sdef["text"];
    console.log("anntext = ",stext)
    var ann = new RcalAnnounce(sname,suid,stext);
    console.log("import announcement ", ann.name,ann.suid,ann.text);
    addAnnounce( ann );

}

//Add announcement to announcement list as an fc event

function addAnnounce( ann ) {
    console.log("add announcement ",ann.name, ann.suid);
    const xclasses='fc-event fc-h-event fc-daygrid-event fc-daygrid-block-event fc-event-draggable'
    const outer=$("<div></div>",
                  {id: "outer_"+ann.name,
                   class: xclasses});
    let inner=$('<div></div>',
                {id: ann.suid,
                 class: 'fc-event-main',
               }).text(ann.text);
    inner[0].style.backgroundColor = "#ffcc66";
    outer.append(inner);
    outer.appendTo('#external-announce-list');
    //
    ann.registered = true;
    Announcements[ann.name] = ann;
}







/////////////////////////////////////////////////////////////////////////////////
//// EVENT Edit Dialog
////    when the user clicks on an event in the calendar, raise this
////  dialog that allows some tweeking, e.g. repeat pattern days of week.

/// TODO:   !STUB! This is quite INCOMPLETE at the moment

function init_evt_modal(evname) {
    let emodal = document.getElementById("evtModal");
    // When the user clicks the event button, adjust header color and open
    let ebtn = document.getElementById("evtBtn");
    ebtn.onclick = function() {
        mdheader = emodal.childNodes[1].childNodes[1];
        mdheader.style.backgroundColor = "blue";
        emodal.style.display = "block";
    }
    // When the user clicks on a close element, close the dialog
    let eclose = document.getElementById("eclose");
    eclose.onclick = function() {
        emodal.style.display = "none";
    }
    return emodal;
}

/// TODO: initialize the event modal dialog
/// var evtModal = init_evt_modal("evtModal")

/////////////////////////////////////////////////////////////////////////////
////  DIALOG TEST JIG -- see eedit.html

init_modals();
init_source_pane();
//load_test_sources();
parse_rsked_sources();
get_announce();

///////// TODO:
/// [x] - Add an "alternate" selector to pick an alternate source.
/// [x] - Show the encoding (ogg, mp3...) in source dialog (read-only)
/// [ ] - Provide a mechanism to delete a source
/// [ ] - Make source tabs draggable to Full Calendar
