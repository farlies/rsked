#!/bin/bash

# Check that all members of the playist are readable Ogg or MP3 data
# Relative pathnames are presumed relative to the MUSICDIR, which
# can be set in the environment of the call to this script if needed,
# defaulting to $HOME/Music.

# Return success if the file named by the argument contains audio data
# in a known format (Ogg, MPEG)
#
function audio_data
{
    ft=$(file -b "$1")
    if [[ "$ft" = *"Vorbis"* ]]; then
        return 0;
    fi
    if [[ "$ft" = *"contains:MPEG"* ]]; then
        return 0;
    fi
    echo "BAD: $ft"
    return 13
}


playlist=$1
declare -i g=0
declare -i b=0
while read entry; do
   if [ ${entry:0:1} = '#'  ]; then continue; fi
   if [ ${entry:0:1} = '/' ]; then # absolute path
        filename=$entry
   else # path relative to MUSICDIR
        filename=${MUSICDIR:=$HOME/Music}/$entry
   fi
   if [ -e "$filename" ]; then
     if audio_data "$filename"; then 
       g=g+1
     else
      echo "UNRECOGNIZED FORMAT: '$filename'"
       b=b+1
     fi
   else
      echo "MISSING: '$filename'"
     b=b+1
   fi
done < $playlist
echo "$playlist :  $g good;  $b bad."
exit $b
