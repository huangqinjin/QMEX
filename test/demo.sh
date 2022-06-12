#!/usr/bin/env bash

cli=${1:-qmex-cli}
file=${2:-$(basename ${BASH_SOURCE%.*}).ini}

if [[ -x /bin/wslpath ]]; then
    cli=$(/bin/wslpath -u $cli)
fi

grep -v '^[;#]' <<- QUERIES | $cli $file
###################################################
##############Begin Queries########################
###################################################

# Query [1]
Grade:2 Subject:Math Score:80   Class Average Weight

# Query [2] (Query [1] With Validation)
Grade:2 Subject:Math Score:80   Class:B Average:80 Weight:0.894

# Query [3]
Grade:1 Subject:Math Score:99   Weight

# Query [4] (Query [3] With Validation)
Grade:1 Subject:Math Score:99   Weight:0.6

# Query [5]
Grade:2 Subject:Art Score:100   Class:A Average:95 Weight


###################################################
################End Queries########################
###################################################
QUERIES

