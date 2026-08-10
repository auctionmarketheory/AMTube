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
import sys, json, os, urllib.request, urllib.parse, random, traceback, ssl

category = "$CATEGORY"
script_dir = "$SCRIPT_DIR"
data_dir = "$DATA_DIR"
thumb_dir = "$THUMB_DIR"
output_file = "$OUTPUT_FILE"
channel_file = os.path.join(script_dir, 'amtube_channels.txt')

# Ignore SSL verification (R36S often has wrong system time causing "certificate is not yet valid")
ssl_ctx = ssl.create_default_context()
ssl_ctx.check_hostname = False
ssl_ctx.verify_mode = ssl.CERT_NONE

# Fallback list - try in order until one works
INVIDIOUS_INSTANCES = [
    "yewtu.be",
    "inv.bp.projectsegfau.lt",
    "invidious.privacydev.net",
    "vid.puffyan.us",
]

def try_fetch(url):
    req = urllib.request.Request(url, headers={'User-Agent': 'AMTube/1.0 (Retro Handheld)'})
    with urllib.request.urlopen(req, timeout=12, context=ssl_ctx) as r:
        return json.loads(r.read().decode('utf-8'))

def build_url(host, category, channel_file):
    if category == 'Subscribed':
        try:
            with open(channel_file, 'r', encoding='utf-8') as f:
                lines = [l.strip() for l in f.readlines() if l.strip()]
            if lines:
                target = random.choice(lines)
                print(f'[AMTube] Zapping to Channel: {target}')
                q = urllib.parse.quote(target)
                return f'https://{host}/api/v1/search?q={q}&type=video'
        except Exception as e:
            print(f'[AMTube] Cannot read channels.txt: {e}')
        return f'https://{host}/api/v1/popular'
    elif category == 'Trending':
        return f'https://{host}/api/v1/popular'
    else:
        q = urllib.parse.quote(category)
        return f'https://{host}/api/v1/search?q={q}&type=video'

# Try each instance until success
data = None
for host in INVIDIOUS_INSTANCES:
    url = build_url(host, category, channel_file)
    print(f'[AMTube] Trying: {url}')
    try:
        data = try_fetch(url)
        print(f'[AMTube] Success with {host}')
        break
    except Exception as e:
        print(f'[AMTube] {host} failed: {e}')
        continue

if data is None:
    print('[AMTube] All instances failed. No internet or all servers down.')
    with open(output_file, 'w') as f:
        f.write('ERROR|NO INTERNET|Check WiFi - All servers failed|/tmp/error.jpg\n')
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

print(f'[AMTube] Done. {count} videos saved.')
PYEOF

exit 0
