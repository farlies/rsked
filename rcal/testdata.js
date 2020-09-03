////////////////////////////////////////////////////////////////////////////////
/// NOTE:
///  In deployment, these vars will be loaded from a json URL on the server,
///  as created by the utility rskrape.pl.
///
///  Assume for now that they are present and structured as illustrated
///  by these examples.  Use the host_ accessor functions to fetch them.

/// Schema:   artist_directory . album_directory . {encoding, duration, tracks}

var gFiles = {
    "Merzbow":
    { "Pulse Demon":
      { "encoding" : "mp3",
        "totalsecs" : "73:17",
        "tracks" :  ["01-Woodpecker No. 1.mp3",
                    "02-Woodpecker No. 2.mp3",
                    "03-Spiral Blast.mp3",
                    "04-My Station Rock.mp3",
                    "05-Ultra Marine Blues.mp3",
                    "06-Tokyo Times Ten.mp3",
                    "07-Worms Plastic Earthbound.mp3",
                    "08-Yellow Hyper Balls.mp3"]},
    },

    "Ravi Coltrane": 
    {"Spirit Fiction":
     {"encoding" : "ogg",
      "totalsecs" : "39:16",
      "tracks" :  ["01-Roads Cross.ogg",
                  "02-Klepto.ogg",
                  "03-Spirit Fiction.ogg",
                  "04-The Change, My Girl.ogg",
                  "05-Who Wants Ice Cream.ogg",
                  "06-Spring & Hudson.ogg"]},

     "Tipis Friction":
     {"encoding" : "ogg",
      "totalsecs" : "39:47",
      "tracks" : ["07-Cross Roads.ogg",
                 "08-Yellow Cat.ogg",
                 "09-Check Out Time.ogg",
                 "10-Fantasm.ogg",
                 "11-Marilyn & Tammy.ogg"]}
    },

    "Miles Davis" : {
        "Dark Magus: Live at Carnegie Hall" : {
            "tracks" : [
                "Disc 1 - 4 - Wili, Part 2.ogg",
                "Disc 1 - 3 - Wili, Part 1.ogg",
                "Disc 2 - 1 - Tatu, Part 1.ogg",
                "Disc 1 - 2 - Moja, Part 2.ogg",
                "Disc 2 - 2 - Tatu, Part 2 (Calypso Frelimo).ogg",
                "Disc 2 - 3 - Nne, Part 1 (Ife).ogg",
                "Disc 1 - 1 - Moja, Part 1.ogg",
                "Disc 2 - 4 - Nne, Part 2.ogg"
            ],
            "encoding" : "ogg"
        },
        "Relaxin' with the Miles Davis Quintet" : {
            "tracks" : [
                "05-It Could Happen to You.ogg",
                "06-Woody'n You.ogg",
                "02-You're My Everything.ogg",
                "04-Oleo.ogg",
                "01-If I Were a Bell.ogg",
                "03-I Could Write a Book.ogg"
            ],
            "totalsecs" : "36:22",
            "encoding" : "ogg"
        },
        "Big Fun-Disc 1" : {
            "encoding" : "ogg",
            "totalsecs" : "49:00",
            "tracks" : [
                "01-Great ExpectationsMulher Laranja.ogg",
                "02-Ife.ogg"
            ]
        },
        "Big Fun-Disc 2" : {
            "encoding" : "ogg",
            "totalsecs" : "49:49",
            "tracks" : [
                "01-Go Ahead John.ogg",
                "02-Lonely Fire.ogg"
            ]
        }
   },

    "Uriah Heep":
    { "Demons and Wizards":
      {"encoding" : "ogg",
       "totalsecs" : "41:40",
       "tracks" : ["01-The Wizard.ogg",
                  "02-Traveller in Time.ogg",
                  "03-Easy Livin.ogg",
                  "04-Poets Justice.ogg",
                  "05-Circle of Hands.ogg",
                  "06-Rainbow Demon.ogg",
                  "07-All My Life.ogg",
                  "08-Paradise-The Spell.ogg"]}
    }
};

/// Available playlists (found under MPD configuration directory).
var gPlaylists = [
    "gonzo.m3u",
    "master.m3u"
];

////////////////////////////////////////////////////////////////////////////////

var rsked_schedule = {
    "encoding" : "UTF-8",
    "schema"   : "2.0",           // NOTE: new Schema 2.0 !
    "version"  : "1.0",

    "sources" :
    {
        "kbem" : {"encoding" : "wfm", "medium": "radio", "location" : 88.5,
                  "alternate" : "master"},

        "kfai" : {"encoding" : "wfm", "medium": "radio", "location" : 90.3,
                  "alternate" : "master"},

        "know" : {"encoding" : "wfm", "medium": "radio", "location" : 91.1,
                  "alternate" : "master"},

        "ksjn" : {"encoding" : "wfm", "medium": "radio", "location" : 99.5,
                  "alternate" : "master"},

        "cms"  : {"encoding" : "mp3", "medium" : "stream",
                  "location" : "http://cms.stream.publicradio.org/cms.mp3",
                  "alternate" : "ksjn",  "repeat" : true},
        
        "nis"  : {"encoding" : "mp3", "medium": "stream", 
                  "location" : "http://nis.stream.publicradio.org/nis.mp3",
                  "alternate" : "know",  "repeat" : true},
        
        "master" : {"encoding" : "ogg", "medium": "playlist", "repeat" : true,
                    "location" : "master.m3u"},

        "gonzo" : {"encoding" : "ogg", "medium": "playlist", "repeat" : true,
                   "location" : "gonzo.m3u"},

        "bigfun1" : {"encoding" : "ogg", "medium": "directory", "repeat" : true,
                     "location" : "Miles Davis/Big Fun-Disc 1"},

        "easylivin" : {"encoding" : "ogg", "medium": "file", "repeat" : false,
                       "location" : "Uriah Heep/Demons and Wizards/03-Easy Livin.ogg"},

        "dnow" : {"encoding" : "mp3", "medium": "stream",  
                  "repeat" : false, "alternate" : "nis", "dynamic" : true,
                  "location" : "https://traffic.libsyn.com/democracynow/dn%Y-%m%d.mp3"},
        
        "%snooze1" : {"encoding" : "ogg", "medium": "file",
                      "location" : "~/.config/rsked/resource/snooze-1hr.ogg" },
        "%resume" : {"encoding" : "ogg", "medium": "file",
                     "location" :  "~/.config/rsked/resource/resuming.ogg" },
        "%revert" : {"encoding" : "ogg", "medium": "file", 
                     "location" :  "~/.config/rsked/resource/reverting.ogg" },
        "%goodam" : {"encoding" : "ogg", "medium": "file",
                     "location" :  "~/.config/rsked/resource/goodam.ogg" },
        "%goodaf" : {"encoding" : "ogg", "medium": "file",
                     "location" :  "~/.config/rsked/resource/goodaf.ogg" },
        "%goodev" : {"encoding" : "ogg", "medium": "file", 
                     "location" :  "~/.config/rsked/resource/goodev.ogg" },
        "%motd"   : {"encoding" : "ogg", "medium": "file",
                     "location" :  "~/.config/rsked/resource/motd.ogg" }
    },

    "dayprograms" : {
    "sunday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "08:00", "program" : "cms" },
        {"start" : "21:00", "program" : "OFF" }
    ],

    "monday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "07:30", "program" : "cms" },
        {"start" : "14:00", "program" : "nis" },
        {"start" : "15:00", "program" : "cms" },
        {"start" : "21:00", "program" : "OFF" }
    ],

    "tuesday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "07:30", "program" : "cms" },
        {"start" : "14:00", "program" : "nis" },
        {"start" : "15:00", "program" : "cms" },
        {"start" : "21:00", "program" : "OFF" }
    ],

    "wednesday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "07:30", "program" : "cms" },
        {"start" : "14:00", "program" : "nis" },
        {"start" : "15:00", "program" : "cms" },
        {"start" : "21:00", "program" : "OFF" }
    ],

    "thursday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "07:30", "program" : "cms" },
        {"start" : "14:00", "program" : "nis" },
        {"start" : "15:00", "program" : "cms" },
        {"start" : "21:00", "program" : "OFF" }
    ],

    "friday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "07:30", "program" : "cms" },
        {"start" : "14:00", "program" : "nis" },
        {"start" : "15:00", "program" : "cms" },
        {"start" : "21:00", "program" : "OFF" }
    ],

    "saturday" : [
        {"start" : "00:00", "program" : "OFF" },
        {"start" : "08:00", "program" : "cms" },
        {"start" : "21:00", "program" : "OFF" }
    ]
    }
};

////////////////////////////////////////////////////////////////////////////////
