#!/bin/bash

# 目标文件的 URL
URL="https://cdn-lfs-us-1.huggingface.co/repos/3c/42/3c429a32fd39f4234ac446273af0c092c59000a64cb19950a9c561a44e41c043/235248dbb95de8afafaa48bfb694ae4d7f3b69c79725276a0a8366239245e26e?response-content-disposition=attachment%3B+filename*%3DUTF-8%27%27yi-34b-chat.Q6_K.gguf%3B+filename%3D%22yi-34b-chat.Q6_K.gguf%22%3B&Expires=1701482779&Policy=eyJTdGF0ZW1lbnQiOlt7IkNvbmRpdGlvbiI6eyJEYXRlTGVzc1RoYW4iOnsiQVdTOkVwb2NoVGltZSI6MTcwMTQ4Mjc3OX19LCJSZXNvdXJjZSI6Imh0dHBzOi8vY2RuLWxmcy11cy0xLmh1Z2dpbmdmYWNlLmNvL3JlcG9zLzNjLzQyLzNjNDI5YTMyZmQzOWY0MjM0YWM0NDYyNzNhZjBjMDkyYzU5MDAwYTY0Y2IxOTk1MGE5YzU2MWE0NGU0MWMwNDMvMjM1MjQ4ZGJiOTVkZThhZmFmYWE0OGJmYjY5NGFlNGQ3ZjNiNjljNzk3MjUyNzZhMGE4MzY2MjM5MjQ1ZTI2ZT9yZXNwb25zZS1jb250ZW50LWRpc3Bvc2l0aW9uPSoifV19&Signature=UxnIdg2X-Md1p4Bgiy8L8Hw18PYB-VE8dxrnb7hV7vmObBIFmhKZ4z4LHAchIulroCQArKZrEEEDlTpFhFZEMBkLM4stSFBfRvPJxHB%7EluUOIZ%7EINFbcbxwgEqvnthTvucDn1sCSfF6j29WpAOyUCDamR4mOa3APSggFcbEblHdHJqtV%7EOUZR9CA6Ycct0FlP1C1LH5MvhDNu929ZnWzqHANp%7E3IEaX2oQINr9YSTCgdRdgS3l9OEd2sl2s3Fji-MxxEwfgOpX9adcVgim8vGtB0G5BPT%7ETn9BPR3NZnRoNBWNoviyKm97Z1Oq4t0kFq30x1teFyqjlo5KIjp3Yy8g__&Key-Pair-Id=KCD77M1F0VK2B"
SSH_KEY="c:/Users/rain_li/Documents/login.pem"
SSH_HOST="root@10.206.138.100"


# 每个块的大小（例如：1GB）
CHUNK_SIZE=2000000000

# 文件总大小
TOTAL_SIZE=$(curl -sI $URL | grep -i Content-Length | awk '{print $2}' | tr -d '\r')

# 计算需要下载的块数
NUM_CHUNKS=$((TOTAL_SIZE / CHUNK_SIZE))
if [ $((TOTAL_SIZE % CHUNK_SIZE)) -ne 0 ]; then
    NUM_CHUNKS=$((NUM_CHUNKS + 1))
fi

echo "Total size: $TOTAL_SIZE bytes, $NUM_CHUNKS chunks."
rm -f part*
# 开始下载
for ((i=0; i<NUM_CHUNKS; i++)); do
    START=$((i * CHUNK_SIZE))
    END=$(((i + 1) * CHUNK_SIZE - 1))
    if [ $END -ge $TOTAL_SIZE ]; then
        END=""
    fi

    FILENAME=$(printf "part%03d" $i)  # 使用 printf 生成带前导零的文件名
    echo "Downloading chunk $((i + 1)) from $START to $END => $FILENAME..."
	rm -f "$FILENAME"
    curl -o "$FILENAME" -r $START-$END $URL
	#sha256sum "$FILENAME" | awk '{print $1}' > "$FILENAME.sha256"
	#scp -i "$SSH_KEY" "part$i" "$FILENAME.sha256" "$SSH_HOST:/home/ai/cache/"
	#scp  "$FILENAME" "$FILENAME.sha256" llm:/home/ai/cache/
	rsync -avz --progress "$FILENAME" llm:/home/ai/cache/

	rm -f "$FILENAME"
done

# 可选：合并文件
#echo "Combining into one file..."
#cat $(ls part*|sort -V) > combined_file

# 清理部分文件
#echo "Cleaning up..."
#rm part*
