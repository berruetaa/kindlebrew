#!/bin/sh
set -eu

URL="https://ve.uy/repo/upstream/gnomegames-1.1.zip"
SHA256="3ac019bcca2634d0cc68ca141462eaa26cf357ee1247c0099fea939f74969448"
TMP="/mnt/us/.kindlebrew-gnomegames"
TARGET="/mnt/us/extensions/gnomegames"
DOC="/mnt/us/documents/GnomeChess.sh"
NEW="${TARGET}.kpm-new.$"
OLD="${TARGET}.kpm-old.$"
DOC_NEW="${DOC}.kpm-new.$"


if [ -e "$TARGET" ] && [ ! -f "$TARGET/.kindlebrew-managed" ]; then
  echo "Existing GnomeGames installation is not managed by Kindlebrew; refusing to overwrite it."
  exit 1
fi

command -v curl >/dev/null 2>&1 || { echo "curl is required."; exit 1; }
command -v unzip >/dev/null 2>&1 || { echo "unzip is required."; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "sha256sum is required."; exit 1; }

rm -rf "$TMP" "$NEW" "$OLD"
rm -f "$DOC_NEW"
mkdir -p "$TMP" /mnt/us/extensions /mnt/us/documents

curl -fL --retry 3 -o "$TMP/gnomegames.zip" "$URL"
echo "$SHA256  $TMP/gnomegames.zip" | sha256sum -c -
unzip -q "$TMP/gnomegames.zip" -d "$TMP/unpacked"

SRC="$TMP/unpacked/gnomegames"
if [ ! -f "$SRC/bin/gnomegames.sh" ] || [ ! -f "$SRC/shortcut_gnomechess.sh" ]; then
  echo "Unexpected GnomeGames archive layout."
  rm -rf "$TMP"
  exit 1
fi

rm -rf "$NEW"
mkdir -p "$NEW"
cp -R "$SRC"/. "$NEW/"
chmod 755 "$NEW/bin/gnomegames.sh" 2>/dev/null || true
chmod 755 "$NEW/shortcut_gnomechess.sh" 2>/dev/null || true
printf '%s\n' 'managed-by=kindlebrew' > "$NEW/.kindlebrew-managed"

if [ ! -f "lib/libGL.so.1" ]; then
  echo "Kindlebrew Chess compatibility shim is missing."
  rm -rf "$TMP" "$NEW"
  exit 1
fi

mkdir -p "$NEW/lib"
if ! cp "lib/libGL.so.1" "$NEW/lib/libGL.so.1"; then
  rm -rf "$TMP" "$NEW"
  exit 1
fi
chmod 755 "$NEW/lib/libGL.so.1" 2>/dev/null || true

cat > "$DOC_NEW" <<'EOF'
#!/bin/sh
# Name: GNOME Chess
# Author: GNOME Games / Kindlebrew
# Icon: data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAABHNCSVQICAgIfAhkiAAAABl0RVh0U29mdHdhcmUAd3d3Lmlua3NjYXBlLm9yZ5vuPBoAAAgTSURBVGiB7VldbBTXFf7uzNx7Z39t7y72YvkPO84mYon5C24w2AuF2EQF+hcQqtRWal4qleQhiYrKA+UhlVMKVUmlKi99ahJFqogIfWorbBJigUR4qdSHIoIJtMBmvcb2/szszNzbh9ldr39ZKF0rEt/u0c6dOTr3fPece+bMLJFS4usMZaUd+F/xhMBK4wmBlcYTAisNbSUmHT45vImqypsAeh3baVE19TaAy5YjThx5/cjnD2OL1PJGdvz4cc0f0N/iXD/cn0jo4VCY+LxeZHM5pCZS8tMLFwzTNN7JzBhHjx07Zldjs6YR8Af0tzo61rya2LFTF8JBLp9FMjUNEIJwOEQOHfqBZ/ST84fHv7gBAD+vxmbN9sDwyeFNjOuvJhI7ddu2kM/nYFk2pJQoFEzcn0ojfT+FRP9OD+f64eGTw5uqsVszAlRV3kwkEhwQcBxnUR2zYCBv5rB9YEAv7pEHopZVqDccjhDbWT61c7ksIuEIAdBbjdGaEXBsp8WjeyCEWFbPdix4vV44ttNSjd2aEVA19XbeyENRlp9SUylyuRyKpfWBqGUKXZ6YSElNXb7web0+pCZSEsDlaozWjIDliBOjo6MmoEBV1UV1ONPh0b349MIFw3LEiWrs1ozAkdePfF4wjdPnR/5uaiqFx+MFpRoIIWCMo74uhFB9BKMj5w3TNN6p9o5c014oM2Mc/fLmzd+9/8F7xp07d6CpDI2RKHyeACZSE3j/g/eM8fEbpzMzxtFqbda0lSjh7VNvP69CXvIHgsrM9DQCwSBmZmYcR8reh+2FIKVcETl56tfnxi5flKn0V/Li2CfiN78d/vBR7KxINwoAppF9Y3R0ZM/YZ2NqwTIcImhVvc98rNjzwFQm2xMMBHOv/OQVUI0ZkzOT3Y9iZ8UIcMZfe2Hr1gAhCvr6+nycaT99FDvLptDGjRt3SSn/9mguLg2Px4Nv7hrAmo5OGKaB7qdi5Gzh4+9s2bJFWpY1R1dV1aeuXLlyfSlbyxKglH77Rz/+oXPw4EEVkJBCovwREhIApNvbiGI1k5AofnHkF79cYFNKiZbVEezavRuQErZtgVGGvm3bYFsC91L3S5pIJZP/vHP71n4Apx6JgKoq3+vt3aLalgUhBaQUFc5LdyxnKxlQrA4gIJAYHx/H/DLdvLoRoVAD4mvXwbLd1Z7JTqP3+S24euUKrt+4ia9SaQQCAUSj0dC9//x7WQJL7oGenp4YY7yuvb0dEmLWubLzEkKUSLi/QkoIKSCEAyEEuM7BGC1LIOBDd1cHXn75+wAIbNttrbO5DAghOHDgIJ55eg18Pg8sy4JKaZgQsjkej4cemoCqqnsTiQFFClleZRdFEqWUkSg6XhkRASEFqKaBMgbKGLiuY108hsGhQYQaIihYxpz5UhNJRKOr0T/Qj3XxGCilEI6gq5qitzVNe2kpP5dMIb/ff2jb9j4upDO7+tIlA+J6Xlp9FNNnNiKuDcYZhOPqdHe1YdPGDVj/3AYYZg7zMguOcJBKJ/FC71Yk7yVhFRykJ6fR3tHuS969sx/An6qOwObNm+tM04yvXbu2nNsg7saUqCBSjEBZyqnmRoAzDsooIpEGdHS0Y+jFPTALBhxn8Yca0zQwOZXG3m/tQ1tbCxjT4A8GIwB2x+NxVnUEbNsebO3okD977Q18eetWxRW58GiZVur3p0+AKMClsUvY3t8PIZxy3i+FbC4DznQM9PeDM4qX9uzl/7h69UYymUwA+Ot8/UUj4PV6D/gCAW6YZjmHXeHzxstLMOiD3+/HTGYGjZFG2Eus/HyYBQPRpiZMT08hGPBjcHCwQAjZt5juAgKEEMV2nCFNoygUCmCUPqRoZSFEgUIITMOAz++HEIu/jVhIwITP50c+b0BIgaGhFzsAVEdg/fr13wiFwoRqGggBKKOgtFLY3PGc69qca0RRoCgKvF4vjLwBVVn8SWw+OOMwDANejxeO46C5uTlYV1enbdiwYf0DCaiqur+1rY0XLAuMcXDGwDkDKwlzhbPZ48rzlaIQBYqiYk1nF86c+TNsx4Gue0A1BkVRQAhxnVBUMMrg9wXQGImCMw8++ugMWlrbMTU1DQmJHTsSucXSaMEm5pwfZB6Pmk2nQRldsFvd9mGRzTzvvOuYuz5dXZ0omBbeffcP6O5+Gj3P9SAUCoFxDkopGOUwTAOTk2l8dnEM1679C83NLWhtbUOp/A0NDbaePfvxdwkhv5JSlitBmQAhhHV2dsaiq5ubfD4vJiYmwBmtcHquu/OJzKVIAMgygYb6esRiMYRXRXD71i2c+8s5mJYJx7LhOA4UVYGqadBUimCwDvF4D8LhCCLhEBoa6iClRCwWY7qud0Wj0R5CyBcAslLKglZ0Xgfg55zve+bZZxXbdsAYXeD4kqu/yDlSEQFVUdHUtAqhUD1WR5uQzWSRN00UzAIcxwYhCjRNg67r8OpeBIJ+1AWD0Dl3C0Ex3fr6tprpdHrf3bt3/whAJYRkShHQAGj19fX7G6NNzOf1oaG+fq7z84mUWs4K1+ffEnTGISUBKTLSdQ+CgSAAUs5/QgACAhACQhR3XD6ngICgqIqhocHQyMjoHrh3ZROARqSUIIToDQ0Nq9rb268DoI/rOb808eMEIcS+du1aTyaTSQLIlN9KEEIYAB8ALwAPAL0oFG6EnOJxLWAV5xIA7OLYKEoeQA7FPbDgtQohRAPAi1JyvlTA/w9ruihmK4G7cCUSJgCzsgqtyHuhx4mv/b+UTwisNP4LlGnw/4m+r1YAAAAASUVORK5CYII=
# DontUseFBInk
TARGET="/mnt/us/extensions/gnomegames"
export LD_LIBRARY_PATH="$TARGET/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec sh "$TARGET/bin/gnomegames.sh" glchess
EOF
chmod 755 "$DOC_NEW" 2>/dev/null || true

had_old=0
if [ -e "$TARGET" ]; then
  if ! mv "$TARGET" "$OLD"; then
    rm -rf "$TMP" "$NEW"
    rm -f "$DOC_NEW"
    exit 1
  fi
  had_old=1
fi

if ! mv "$NEW" "$TARGET"; then
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  rm -rf "$TMP" "$NEW"
  rm -f "$DOC_NEW"
  exit 1
fi

if ! mv -f "$DOC_NEW" "$DOC"; then
  rm -rf "$TARGET"
  [ "$had_old" -eq 0 ] || mv "$OLD" "$TARGET" 2>/dev/null || true
  rm -rf "$TMP"
  rm -f "$DOC_NEW"
  exit 1
fi

rm -rf "$OLD" "$TMP" 2>/dev/null || true
echo "GNOME Chess installed. Open Gnome Chess from the Kindle library or run ;kpm launch chess."
