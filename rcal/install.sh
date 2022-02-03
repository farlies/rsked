#!/bin/bash

# Install kit to web root; needs to be run as root

#---------------------------------------------------
#  You might need to customize these variables:

WEBROOT=/var/www
WEBUSER=www-data
WEBGROUP=www-data

FULLCALZIP=./fullcalendar-5.2.0.zip
JQUERY=./jquery.js

PAGEDIR=${WEBROOT}/html
BINDIR=${WEBROOT}/misc-bin
UPDIR=${PAGEDIR}/upload
IMGDIR=${PAGEDIR}/images
LIBDIR=${PAGEDIR}/lib

#---------------------------------------------------

SOURCES="findlogs.php getschedule.php images localparams.php log2table.php logview.html logview.js lstyles.css newsked.php rcal.html rcal.js styles.css w3.css"


mkdir -p ${WEBROOT}
mkdir -p ${PAGEDIR}
mkdir -p ${BINDIR}
mkdir -p ${LIBDIR}

# uploads subdirectory
mkdir -p ${UPDIR}
chown ${WEBUSER}:${WEBGROUP} ${UPDIR}
chmod ug+rwx ${UPDIR}

# install fullcalendar
if [ -e ${FILLCALZIP} ]; then
    unzip ${FULLCALZIP} 'lib/*' -d ${PAGEDIR}
    mv ${PAGEDIR}/lib ${PAGEDIR}/fullcalendar
else
    echo "Warning: ${FULLCALZIP} not found--skipping"
fi

# install jquery
if [ -e ${JQUERY} ]; then
    cp -v ${JQUERY} ${PAGEDIR}
else
    echo "Warning: ${JQUERY} not found--skipping"
fi

# install web pages
cp -rv ${SOURCES} ${PAGEDIR}/

cp -v images/favicon.ico ${WEBROOT}/

# intall binaries
cp -rv misc-bin/* ${BINDIR}/
