#!/bin/bash
# AMTube Backend Script - Data Middleware
# Kéo dữ liệu từ Invidious API, phân trang, lưu text cho C++ đọc.

DATA_DIR="/tmp/yt_data"
THUMB_DIR="/tmp/yt_thumbs"
OUTPUT_FILE="${DATA_DIR}/yt_data.txt"

mkdir -p "$DATA_DIR"
mkdir -p "$THUMB_DIR"

CATEGORY="Trending"
RELOAD=0

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --category) CATEGORY="$2"; shift ;;
        --reload) RELOAD=1 ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

# If reload, delete old thumbs to prevent 'Rác trộn'
if [ "$RELOAD" -eq 1 ]; then
    rm -rf "${THUMB_DIR:?}/"* 2>/dev/null
    rm -f "$OUTPUT_FILE"
fi

if [ -f "$OUTPUT_FILE" ] && [ "$RELOAD" -eq 0 ]; then
    echo "[AMTube] Data already exists. No need to fetch."
    exit 0
fi

echo "[AMTube] Fetching data for category: $CATEGORY"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Use python3 to handle all the complex API calling, JSON parsing and thumbnail downloading
python3 -c "
import sys, json, os, urllib.request, urllib.parse, random, traceback

category = sys.argv[1]
script_dir = sys.argv[2]
data_dir = '${DATA_DIR}'
thumb_dir = '${THUMB_DIR}'
output_file = '${OUTPUT_FILE}'
channel_file = os.path.join(script_dir, 'amtube_channels.txt')

api_url = 'https://vid.puffyan.us/api/v1/popular'
if category == 'Subscribed':
    try:
        with open(channel_file, 'r', encoding='utf-8') as f:
            lines = [l.strip() for l in f.readlines() if l.strip()]
        if lines:
            target_channel = random.choice(lines)
            print(f'[AMTube] Zapping to Channel: {target_channel}')
            safe_query = urllib.parse.quote(target_channel)
            api_url = f'https://vid.puffyan.us/api/v1/search?q={safe_query}&type=video'
        else:
            api_url = 'https://vid.puffyan.us/api/v1/popular'
    except Exception as e:
        print(f'[AMTube] Error reading channels.txt: {e}')
        traceback.print_exc()
        api_url = 'https://vid.puffyan.us/api/v1/popular'
elif category != 'Trending':
    # Search API
    safe_query = urllib.parse.quote(category)
    api_url = f'https://vid.puffyan.us/api/v1/search?q={safe_query}&type=video'

try:
    req = urllib.request.Request(api_url, headers={'User-Agent': 'AMTube/1.0 (Retro Handheld)'})
    with urllib.request.urlopen(req, timeout=10) as response:
        html = response.read()
        data = json.loads(html)
except Exception as e:
    print(f'[AMTube] Error fetching API: {e}')
    traceback.print_exc()
    # Ghi file rỗng hoặc báo lỗi cho C++ biết
    with open(output_file, 'w') as f:
        f.write('ERROR|NO_INTERNET|Check WiFi Connection|/tmp/error.jpg\n')
    sys.exit(1)

# Invidious returns list for /popular, but dict for /search?q=
videos = []
if isinstance(data, dict) and 'items' in data:
    videos = data['items']
elif isinstance(data, list):
    videos = data

if not videos:
    print('[AMTube] No videos found.')
    sys.exit(0)

count = 0
try:
    with open(output_file, 'w', encoding='utf-8') as out_f:
    for vid in videos:
        if count >= 10:
            break
        if vid.get('type') != 'video' and 'videoId' not in vid:
            continue
        
        vid_id = vid.get('videoId')
        # Loại bỏ các ký tự có thể gây phá vỡ định dạng phân cách |
        title = vid.get('title', 'Unknown Title').replace('|', '-').replace('\n', ' ')
        author = vid.get('author', 'Unknown').replace('|', '-').replace('\n', ' ')
        
        # Lấy ảnh thumbnail chất lượng vừa phải
        thumb_url = ''
        if 'videoThumbnails' in vid and len(vid['videoThumbnails']) > 0:
            for t in vid['videoThumbnails']:
                if t.get('quality') == 'mqdefault' or t.get('quality') == 'hqdefault':
                    thumb_url = t.get('url')
                    break
            if not thumb_url:
                thumb_url = vid['videoThumbnails'][0].get('url')
                
        local_thumb = f'{thumb_dir}/{vid_id}.jpg'
        
        # Ghi kết nối C++
        out_f.write(f'{vid_id}|{title}|{author}|{local_thumb}\n')
        
        # Tải ảnh ngầm bằng curl (curl có sẵn, wget có thể thiếu)
        if thumb_url:
            os.system(f'curl -sL \"{thumb_url}\" -o \"{local_thumb}\"')
            
        count += 1
except Exception as e:
    print(f'[AMTube] Error parsing videos or downloading thumbs: {e}')
    traceback.print_exc()

print(f'[AMTube] Successfully parsed and downloaded {count} videos.')
" "$CATEGORY" "$SCRIPT_DIR"

exit 0
