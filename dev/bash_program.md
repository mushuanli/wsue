* debug
```
set -x -u -e
```
---

* define and use hash of array:
```
declare -a RBCMD_scan=("scan"	    "scan"                      "endpoint"	"\"computer_id\": \"$DEF_COMPUTER_ID\",\"_string\": \"$DEF_STRING\"")

declare -A RBCMD=([scan]="(${RBCMD_scan[*]@Q})" ... )

    local cmd=${RBCMD[$1]}
    if [[ ! -z "$cmd" ]] ; then
        declare -a item=$cmd
```        
 --------------------
 
* return function result by param - function param reference
```
function curl_get() {
    local -n inner_output=$1
    inner_output=('a' 'b')
}

    local retvals
    curl_get retvals '/hello'
    echo "HELLO: ${retvals[0]}      ${retvals[1]}"
```
----------------------
