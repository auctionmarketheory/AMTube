#!/bin/bash
# AMTube Backend Script - Direct YouTube Scraper
# Bypasses all Invidious/Piped API blocks

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
import sys, json, os, urllib.request, urllib.parse, random, traceback, re, ssl

category = "$CATEGORY"
script_dir = "$SCRIPT_DIR"
data_dir = "$DATA_DIR"
thumb_dir = "$THUMB_DIR"
output_file = "$OUTPUT_FILE"
channel_file = os.path.join(script_dir, 'amtube_channels.txt')

# Ignore SSL verification due to R36S time desync
ssl_ctx = ssl.create_default_context()
ssl_ctx.check_hostname = False
ssl_ctx.verify_mode = ssl.CERT_NONE

# Determine search query
search_query = category
if category == 'Subscribed':
    try:
        with open(channel_file, 'r', encoding='utf-8') as f:
            lines = [l.strip() for l in f.readlines() if l.strip()]
        if lines:
            target = random.choice(lines)
            print(f'[AMTube] Zapping to Channel: {target}')
            search_query = target
    except Exception as e:
        print(f'[AMTube] Cannot read channels.txt: {e}')
        search_query = 'Trending'

url = f'https://www.youtube.com/results?search_query={urllib.parse.quote(search_query)}'
print(f'[AMTube] Scraping URL: {url}')

req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'})

try:
    with urllib.request.urlopen(req, timeout=15, context=ssl_ctx) as r:
        html = r.read().decode('utf-8')
except Exception as e:
    print(f'[AMTube] Error fetching YouTube: {e}')
    traceback.print_exc()
    with open(output_file, 'w') as f:
        f.write('ERROR|NO INTERNET|Check WiFi or YouTube blocked|/tmp/error.jpg\n')
    sys.exit(1)

# Extract JSON from HTML
match = re.search(r'ytInitialData\s*=\s*(\{.+?\});', html)
if not match:
    print('[AMTube] Error: ytInitialData not found in HTML')
    with open(output_file, 'w') as f:
        f.write('ERROR|PARSE FAILED|YouTube changed layout|/tmp/error.jpg\n')
    sys.exit(1)

data = json.loads(match.group(1))
raw_videos = []

# Recursive extractor to find all videoRenderer objects
def extract_videos(obj):
    if isinstance(obj, dict):
        if 'videoRenderer' in obj:
            raw_videos.append(obj['videoRenderer'])
        for k, v in obj.items():
            extract_videos(v)
    elif isinstance(obj, list):
        for item in obj:
            extract_videos(item)

extract_videos(data)
print(f'[AMTube] Found {len(raw_videos)} raw videos.')

if not raw_videos:
    print('[AMTube] No videos found.')
    sys.exit(0)

count = 0
try:
    with open(output_file, 'w', encoding='utf-8') as out_f:
        for vid in raw_videos:
            if count >= 10:
                break
            
            vid_id = vid.get('videoId', '')
            if not vid_id:
                continue

            # Parse title
            title = 'Unknown Title'
            title_runs = vid.get('title', {}).get('runs', [])
            if title_runs:
                title = ''.join([r.get('text', '') for r in title_runs])
            title = title.replace('|', '-').replace('\n', ' ')

            # Parse author
            author = 'Unknown'
            author_runs = vid.get('ownerText', {}).get('runs', [])
            if author_runs:
                author = ''.join([r.get('text', '') for r in author_runs])
            author = author.replace('|', '-').replace('\n', ' ')

            # Parse thumbnail
            thumb_url = ''
            thumbs = vid.get('thumbnail', {}).get('thumbnails', [])
            if thumbs:
                # Use the last one (usually highest resolution available)
                thumb_url = thumbs[-1].get('url', '').split('?')[0]
                # Fallback to hqdefault to save RAM/Network if available
                for t in thumbs:
                    if 'hqdefault' in t.get('url', ''):
                        thumb_url = t.get('url', '').split('?')[0]
                        break

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
