#!/bin/bash
# AMTube Backend Script - Data Middleware

DATA_DIR="/tmp/yt_data"
THUMB_DIR="/tmp/yt_thumbs"
OUTPUT_FILE="${DATA_DIR}/yt_data.txt"

mkdir -p "$DATA_DIR"
mkdir -p "$THUMB_DIR"

CATEGORY="Trending"
RELOAD=0

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --category) CATEGORY="$2"; shift ;;
        --reload) RELOAD=1 ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

if [ "$RELOAD" -eq 1 ]; then
    rm -rf "${THUMB_DIR:?}/"* 2>/dev/null
    rm -f "$OUTPUT_FILE"
fi

if [ -f "$OUTPUT_FILE" ] && [ "$RELOAD" -eq 0 ]; then
    echo "[AMTube] Data already exists."
    exit 0
fi

echo "[AMTube] Fetching data for category: $CATEGORY"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

python3 << PYEOF
import sys, json, os, urllib.request, urllib.parse, random, traceback

category = "$CATEGORY"
script_dir = "$SCRIPT_DIR"
data_dir = "$DATA_DIR"
thumb_dir = "$THUMB_DIR"
output_file = "$OUTPUT_FILE"
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
    safe_query = urllib.parse.quote(category)
    api_url = f'https://vid.puffyan.us/api/v1/search?q={safe_query}&type=video'

print(f'[AMTube] API URL: {api_url}')

try:
    req = urllib.request.Request(api_url, headers={'User-Agent': 'AMTube/1.0 (Retro Handheld)'})
    with urllib.request.urlopen(req, timeout=15) as response:
        data = json.loads(response.read().decode('utf-8'))
except Exception as e:
    print(f'[AMTube] Error fetching API: {e}')
    traceback.print_exc()
    with open(output_file, 'w') as f:
        f.write('ERROR|NO_INTERNET|Check WiFi|/tmp/error.jpg\n')
    sys.exit(1)

videos = []
if isinstance(data, dict) and 'items' in data:
    videos = data['items']
elif isinstance(data, list):
    videos = data

print(f'[AMTube] Got {len(videos)} raw items.')

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

            vid_id = vid.get('videoId', '')
            if not vid_id:
                continue

            title  = vid.get('title',  'Unknown Title').replace('|', '-').replace('\n', ' ')
            author = vid.get('author', 'Unknown').replace('|', '-').replace('\n', ' ')

            thumb_url = ''
            thumbs = vid.get('videoThumbnails', [])
            for t in thumbs:
                if t.get('quality') in ('mqdefault', 'hqdefault'):
                    thumb_url = t.get('url', '')
                    break
            if not thumb_url and thumbs:
                thumb_url = thumbs[0].get('url', '')

            local_thumb = f'{thumb_dir}/{vid_id}.jpg'
            out_f.write(f'{vid_id}|{title}|{author}|{local_thumb}\n')

            if thumb_url:
                os.system(f'curl -sL "{thumb_url}" -o "{local_thumb}"')

            count += 1

except Exception as e:
    print(f'[AMTube] Error writing output: {e}')
    traceback.print_exc()

print(f'[AMTube] Done. {count} videos saved to {output_file}')
PYEOF

exit 0
