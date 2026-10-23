FROM alpine:latest

RUN apk upgrade --no-cache \
    && apk add --no-cache \
        rsvg-convert \
        imagemagick

COPY <<'EOF' /usr/local/bin/render-icon
#!/bin/sh
set -eu -o pipefail
if [ $# -ne 3 ]; then
    echo "Usage: render-icon <input.svg> <small.svg> <output.ico>" >&2
    exit 2
fi
source=$1
small=$2
destination=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
# The small artwork is pixel-snapped, so render it directly; resampling would blur its edges.
for size in 16 20 24; do
    rsvg-convert --width $size --height $size "$small" \
        | magick png:- "PNG32:$work/$(printf %03d "$size").png"
done
for size in 32 40 48 64 96 128 256; do
    rsvg-convert --width $((size * 4)) --height $((size * 4)) "$source" \
        | magick png:- -filter Lanczos -resize "${size}x${size}" "PNG32:$work/$(printf %03d "$size").png"
done
mkdir -p "$(dirname "$destination")"
# ImageMagick records every PNG frame as 256 px in the directory, so only the 256 px frame may be PNG.
magick "$work"/*.png -define icon:png-compression-size=256 "ICO:$destination"
echo "Created $destination: 16, 20, 24 px from $small; 32, 40, 48, 64, 96, 128, 256 px from $source (RGBA)"
EOF
# Windows checkouts give the heredoc CRLF line endings.
RUN sed -i 's/\r$//' /usr/local/bin/render-icon \
    && chmod +x /usr/local/bin/render-icon

ENTRYPOINT ["/usr/local/bin/render-icon"]