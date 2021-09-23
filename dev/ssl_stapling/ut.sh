#!/bin/bash

function check_ocsp(){
    local host=$1
    local certfile=$2
    local chainfile=$3

    if [ -z "$certfile" ] ; then
        certfile="${host}.pem"
    fi
    if [ -z "$chainfile" ] ; then
        chainfile="chain.pem"
    fi
    
    echo "============ will check $host ocsp ($certfile/$chainfile)"

    openssl s_client -connect ${host}:443 < /dev/null 2>&1 |  sed -n '/-----BEGIN/,/-----END/p' > $certfile
    openssl s_client -showcerts -connect ${host}:443 < /dev/null 2>&1 |  sed -n '/-----BEGIN/,/-----END/p' > $chainfile

    #read -p "manual edit $chainfile to only keep last record, after finish press enter"
    csplit -f chain- $chainfile '/-----BEGIN CERTIFICATE-----/' '{*}'
    local chain=$(ls chain-[0-9][0-9] | sort | tail -1)
    mv $chain $chainfile
    rm -f chain-[0-9][0-9]
    local ocspurl=$(openssl x509 -noout -ocsp_uri -in $certfile)
    echo "============ check server: $ocspurl"
    openssl ocsp -issuer $chainfile -cert $certfile -text -url $ocspurl
}

function check_ocsp_stagling(){
    local host=$1
    echo "============ will   check server ocsp stagling: $host"
    echo QUIT | openssl s_client -connect ${host}:443 -status 2> /dev/null | grep -A 17 'OCSP response:' | grep -B 17 'Next Update'
    echo "============ finish check server ocsp stagling: $host"
}

case $1 in
    c)
        check_ocsp $2
        ;;
    s)
        check_ocsp_stagling $2
        ;;

    b)
        ./sc www.badu.com
        ;;

    m)
        ./sc www.maxcdn.com
        ;;

    d)
        echo "decode P12 file to domain.{crt,rsa}"
        P12file=$2
        openssl pkcs12 -in $P12file -nodes -clcerts -nokeys -out domain.crt
        openssl pkcs12 -in $P12file -nodes -nocerts -nodes -out domain.rsa
        openssl rsa -in domain.rsa -out domain-key.pem
        cat domain.crt domain-key.pem > domain.pem
        echo 'verify key'
        openssl s_client -connect gateway.push.apple.com:2195 -cert domain.crt -key domain-key.pem
        ;;

    u)
        certfile=$2
        cafile=$3
        echo "update vosp stapling by $certfile $cafile"
        ocspurl=$(openssl x509 -noout -ocsp_uri -in $certfile)
        host=$(echo $ocspurl|sed 's#http://##')
        echo "update $ocspurl from $host"
        openssl ocsp -issuer $cafile -cert $certfile -url $ocspurl -header Host=$host -no_nonce -noverify -respout $host.ocsp
        ;;

    p)
        keyfile=$2
        hostfile=$3
        certfile=$4
        chainfile=$5
        echo "key: $keyfile $hostfile cer:$certfile chain:$chainfile"
        # convert pem to private key: openssl rsa -in us.vmi6demo.trendmicro.com.crt -out private.key
        cat $hostfile $certfile $chainfile > DefaultTempCert3to1.crt 
        #openssl pkcs12 -export -out f.p12 -inkey $keyfile -in DefaultTempCert3to1.crt -password  pass:
        openssl pkcs12 -export -out ../../f.p12 -inkey $keyfile -in DefaultTempCert3to1.crt -chain -CAfile $chainfile -password pass:123456
        #rm -f DefaultTempCert3to1.crt 
        #openssl pkcs12 -export  -inkey $keyfile -in $certfile -certfile $chainfile -out f.p12
        ;;

    *)
        echo "c hostname:       check website ocsp status"
        echo "s hostname:       check ocsp stagling status"
        echo "d filename.p12:   decode P12 file"
        echo "u cer.crt ca.pem: download ocsp file"
        echo "p priv.crt host.pem cert.pem chain.pem: pack ../../f.p12 file"
        ;;
esac
