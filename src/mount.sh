#!/bin/bash

# This file contains DTV frequencies from broadcasters
# based in Sao Paulo/SP, Brazil.

function valgrind_run_gdb()
{
	VALGRIND_LOGFILE=/tmp/demuxfs-valgrind.log
	DEMUXFS_LOGFILE=/tmp/demuxfs-run.log
	VALGRIND_SUPRESSIONS_FILE=supressions.txt
	
	[ ! -e $VALGRIND_SUPRESSIONS_FILE ] && touch $VALGRIND_SUPRESSIONS_FILE
	rm -f $VALGRIND_LOGFILE
	valgrind \
		--leak-check=full \
		--leak-resolution=high \
		--show-reachable=yes \
		--trace-children=no \
		--db-attach=yes \
		--gen-suppressions=yes \
		--suppressions=$VALGRIND_SUPRESSIONS_FILE \
		$@
}

function playFromFrontend() 
{
	local frequency=
	local channels_saopaulo=(
		"479142857:Mega TV"
		"491142857:TV Gazeta"
		"497142857:Globo SP"
		"509142857:Rede Record"
		"521142857:Rede 21"
		"527142857:Band"
		"533142857:TV Cultura"
		"545142857:CNT Americana"
		"557142857:SBT SP"
		"563142857:Rede TV"
		"569142857:RIT"
		"575142857:Ideal TV"
		"587142857:Top TV"
		"623142857:Rede Vida"
		"635142857:TV Aparecida"
		"647142857:Record News"
		"671142857:NGT"
		"683142857:Terra Viva"
		"719142857:TVZ"
		"725142857:Rede Brasil de Televisao"
		"749142857:TV Mackenzie"
		"755142857:TV Camara"
		"767142857:TV Brasil"
		"773142857:TV Justica"
	)

    options=`for i in "${channels_saopaulo[@]}"; do 
             freq=$(echo -n "$i" | cut -d: -f1); 
             name=$(echo -n "$i" | cut -d: -f2 | sed 's, ,_,g');
             echo -n "$freq" "$name ";
             done`
    frequency=`dialog --stdout --title 'Channel list' --menu 'Select broadcaster' 0 0 0 $options`

	local backend=$PWD/backends/.libs/liblinuxdvb.so.0.0.0
	valgrind_run_gdb \
	./demuxfs \
		-o parse_pes=1 \
		-o backend=$backend \
		-o frequency=$frequency \
		-f \
		$@ \
		~/Mount
}

ulimit -c unlimited
fusermount -u ~/Mount 2> /dev/null
playFromFrontend $@
