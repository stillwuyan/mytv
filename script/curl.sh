#!/bin/bash

output=$(dirname $0)/../output

function search()
{
    encoded_keyword=$(echo -n "$2" | jq -sRr @uri)
    curl -L "${1}?ac=videolist&wd=${encoded_keyword}" \
      -H 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36' \
      -H 'Accept: application/json' \
      -H "sec-ch-ua: \"Chromium\";v=\"142\", \"Google Chrome\";v=\"142\", \"Not_A Brand\";v=\"99\"" \
      -H "sec-ch-ua-platform: \"Windows\"" \
      -o ${output}/tmp.txt

    cat ${output}/tmp.txt | jq . > ${output}/result.json
    rm ${output}/tmp.txt
}

function download()
{
    curl -L 'https://pz.v88.qzz.io/?format=0&source=jin18' \
      -H 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36' \
      -H 'Accept: application/json' \
      -o ${output}/tmp.txt

    cat ${output}/tmp.txt | jq . > ${output}/result.json
    rm ${output}/tmp.txt
}

#download

#search "https://iqiyizyapi.com/api.php/provide/vod" "凡人修仙传"

#search "https://caiji.dbzy5.com/api.php/provide/vod" "凡人修仙传"

search "https://lovedan.net/api.php/provide/vod" "凡人修仙传"
