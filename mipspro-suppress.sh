#!/usr/freeware/bin/bash

suppress() {
    print=1 
    first=0
    while read line
    do
        if [ "$first" = "0" ]
        then
            if [ "$line" = "Cannot find SERVER hostname in network database (-14,7:2) No such file or directory" ] || \
               [ "$line" = "No such feature exists (-5,116)" ]

            then
                first=1
		print=0
            fi
        fi
        if [ "$print" -eq "1" ]
        then
            echo $line
        else
            if [ "$line" = "Graphics support customer then contact your local support provider." ] 
            then
		# Read one extra (empty) line after the final message, then display everything after that
                read line
                print=1 
            fi
        fi
    done
}

COMMAND=$(basename $0)
/usr/bin/$COMMAND "$@" 2> >(suppress) >&2

# If the contents of stderr arrive after a delay, wait for the text to appear before going back to prompt
#RET=$?
#[ $RET != 0 ] && sleep 1
#exit $RET
