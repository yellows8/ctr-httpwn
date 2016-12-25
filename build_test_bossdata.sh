# Usage: ./build_test_bossdata.sh {additional param(s) for bosstool}
# Requires bosstool from: https://github.com/yellows8/3dscrypto-tools
# Builds the bossdata files @ web/boss/ used by ctr-httpwn boss_test().

params="--input=bossinput --build --nsdataid=0x58584148"
echo -n "Hello" > bossinput
mkdir web/boss
bosstool --output=web/boss/bossdata_JPN --programID=0004003000008202 $params $1
bosstool --output=web/boss/bossdata_USA --programID=0004003000008f02 $params $1
bosstool --output=web/boss/bossdata_EUR --programID=0004003000009802 $params $1
bosstool --output=web/boss/bossdata_CHN --programID=000400300000A102 $params $1
bosstool --output=web/boss/bossdata_KOR --programID=000400300000A902 $params $1
bosstool --output=web/boss/bossdata_TWN --programID=000400300000B102 $params $1
bosstool --output=web/boss/bossdata_USA --programID=0004003000008f02 $params $1
